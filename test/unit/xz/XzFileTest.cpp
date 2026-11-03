#include "xz/XzFile.h"

#include "support/StoredXz.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

using scrap::TestSupport::TempDirectory;
using scrap::Xz::CannotReadFile;
using scrap::Xz::ConsumerStopped;
using scrap::Xz::CorruptData;
using scrap::Xz::decodeFile;
using scrap::Xz::NotAnXzFile;
using scrap::Xz::TruncatedData;

namespace {

// Written by the xz command: "hello, xz\n".
constexpr std::string_view OneBlock = "fd377a585a000004e6d6b44604c00e0a21011c0000000000000000008396ba3201000968656c6c6f2c20787a0a000000eb12"
                                      "b68bfc71df2800012a0a1d9038af1fb6f37d010000000004595a";

// Two xz files one after the other: "hello, xz\n" and "second\n".
constexpr std::string_view TwoStreams = "fd377a585a000004e6d6b44604c00e0a2101160000000000000000004a06982501000968656c6c6f2c20787a0a000000eb12"
                                        "b68bfc71df2800012a0a1d9038af1fb6f37d010000000004595afd377a585a000004e6d6b44604c00b072101160000000000"
                                        "00000000590817e60100067365636f6e640a000031bdb7f95cc0f16d00012707ed9227641fb6f37d010000000004595a";

// xz --block-size=1MiB: three blocks holding what threeMegabytes() returns.
constexpr std::string_view ThreeBlocks = "fd377a585a000004e6d6b4460200210116000000742fe5a3efffff00f45d00309888983ecbe26f34b34c115f19e4bd173a38"
                                         "0166a2d159709a6fdc87a7df9f4ff5135ceea44995ab5d7e47966ab343bbbc4025d0cff283ccf85d04a4e258e402694100c3"
                                         "d26e94899586513816ff157b23c75c7e4534d8ff282421eb4fb55ea2b575776b4ae60205442d28c77574c28bb64ecdff9528"
                                         "460fa9fc7cdefb9a302e56c08f85f38381c065c42553f8f591363105a5b0ee6fc1704d470cd19111aaad601dbaceb127185c"
                                         "5986e9665258bee976ac59e4e55b0508f9c7daadfcfb522b74cd1e5b2042f9dd533df82964093b80cb2a6cdfb53bf0c4bd2e"
                                         "5faa0f3e4b664290130eff1093f8717859f80bc94fbd3300000079c88ca10fee987a0200210116000000742fe5a3efffff00"
                                         "f45d003b9e0b87fdfbfdaf4d654b4666930cd3dae137496a7d3197dd5b14825653a72678e68ec10a73e34d979707bf704a30"
                                         "b0e2d04ce25b788fa952753806a2e5e27ef2fff94b666b2a3cd62c65b838d6cc19f7b2126c9430d466d8688c43d4526e981d"
                                         "9eb0f2a5f04ce3cf0022fec7061ec8d786704d470cd19111aaad601dbaceb127185c5986e9665258bee976ac59e4e55b0508"
                                         "f9c7daadfcfb522b74cd1e5b2042f9dd533df82964093b80cb2a6cdfb53bf0c4bd2e5faa0f3e4b664290130eff1093f87178"
                                         "59f80bcdff9528460fa9fc7cdefb9a302e56c08f85f38381c065c42553f8f591363105a5b0ee6fc17049ad92880000001e67"
                                         "335309fd0f570200210116000000742fe5a3edc6bf00df5d00399d0affccdd2a4e29a14e9579fbe87c6cc250d1f8a7e58f79"
                                         "2009a6c50a25106d8cf753b5cd92bb735e0be2c8ac46178cc9eccc2bc1376cd06edb3dc4fe0175de243a82ad520170a4c978"
                                         "5b8113976ef3a72e394eec5d0801488c520e841f2e79b013921aa0b8c75df6b7a2b06969225396d925da0cb2e2d2e446f654"
                                         "f088752c30dbc2505491229706035fc9703b14c0323798a51501a5e8a3092aecaaff5a26ac720c1692a7be398f24e1709ea7"
                                         "235fec28cb85d195988a7e2a91f22775f719c006984d98fdd8afd5900fc42553f8f591363105a5b0ee6fc12400f2c5000000"
                                         "43248d64fa4b947a000390028080409002808040fb01c08d370000002856802d09f462e6050000000004595a";

// xz --check=none: "hello, xz\n".
constexpr std::string_view NoCheck = "fd377a585a000000ff12d94104c00e0a2101160000000000000000004a06982501000968656c6c6f2c20787a0a0000000001"
                                     "220a151ae16706729e7a010000000000595a";

std::string bytesFromHex(std::string_view hex)
{
    std::string bytes;
    for (std::size_t start = 0; start < hex.size(); start += 2) {
        bytes.push_back(static_cast<char>(std::stoi(std::string{ hex.substr(start, 2) }, nullptr, 16)));
    }
    return bytes;
}

std::string threeMegabytes()
{
    std::string text(3'000'000, '\0');
    for (std::size_t position = 0; position < text.size(); ++position) {
        text[position] = static_cast<char>('a' + (position % 26));
    }
    return text;
}

class XzFileTest : public ::testing::Test {
protected:
    [[nodiscard]] std::filesystem::path fileWith(std::string_view content) const
    {
        return _directory.writeFile("file.xz", content);
    }

    TempDirectory _directory;
};

auto collectInto(std::string& decoded)
{
    return [&decoded](std::string_view piece) {
        decoded.append(piece);
        return true;
    };
}

}  // anonymous namespace

TEST_F(XzFileTest, DecodesAFile)
{
    std::string decoded;

    const auto result = decodeFile(fileWith(bytesFromHex(OneBlock)), collectInto(decoded));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(decoded, "hello, xz\n");
}

TEST_F(XzFileTest, DecodesAFileOfManyBlocksLargerThanOnePiece)
{
    std::string decoded;
    std::size_t pieces = 0;

    const auto result = decodeFile(fileWith(bytesFromHex(ThreeBlocks)), [&](std::string_view piece) {
        decoded.append(piece);
        ++pieces;
        return true;
    });

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(decoded, threeMegabytes());
    EXPECT_GT(pieces, 1U);
}

TEST_F(XzFileTest, DecodesFilesOneAfterTheOther)
{
    std::string decoded;

    const auto result = decodeFile(fileWith(bytesFromHex(TwoStreams)), collectInto(decoded));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(decoded, "hello, xz\nsecond\n");
}

TEST_F(XzFileTest, DecodesAFileWithoutAnIntegrityCheck)
{
    std::string decoded;

    const auto result = decodeFile(fileWith(bytesFromHex(NoCheck)), collectInto(decoded));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(decoded, "hello, xz\n");
}

TEST_F(XzFileTest, ReportsAMissingFile)
{
    std::string decoded;

    const auto result = decodeFile(_directory.path() / "missing.xz", collectInto(decoded));

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<CannotReadFile>(result.error()));
    EXPECT_EQ(std::get<CannotReadFile>(result.error()).code, std::make_error_code(std::errc::no_such_file_or_directory));
}

TEST_F(XzFileTest, ReportsAFileThatIsNotXz)
{
    std::string decoded;

    const auto result = decodeFile(fileWith("this is plain text, not an xz file\n"), collectInto(decoded));

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<NotAnXzFile>(result.error()));
}

TEST_F(XzFileTest, ReportsAnEmptyFileAsTruncated)
{
    std::string decoded;

    const auto result = decodeFile(fileWith(""), collectInto(decoded));

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<TruncatedData>(result.error()));
}

TEST_F(XzFileTest, ReportsATruncatedFile)
{
    std::string content = bytesFromHex(OneBlock);
    content.resize(content.size() - 1);
    std::string decoded;

    const auto result = decodeFile(fileWith(content), collectInto(decoded));

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<TruncatedData>(result.error()));
}

TEST_F(XzFileTest, ReportsDataThatFailsItsIntegrityCheck)
{
    std::string content = bytesFromHex(OneBlock);
    const std::size_t firstByteOfText = content.find("hello");
    ASSERT_NE(firstByteOfText, std::string::npos);
    content[firstByteOfText] = 'j';
    std::string decoded;

    const auto result = decodeFile(fileWith(content), collectInto(decoded));

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<CorruptData>(result.error()));
}

TEST_F(XzFileTest, ReportsTooLittleDataAfterTheEndOfTheFileAsTruncated)
{
    std::string decoded;

    const auto result = decodeFile(fileWith(bytesFromHex(OneBlock) + "trailing"), collectInto(decoded));

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<TruncatedData>(result.error()));
}

TEST_F(XzFileTest, ReportsMoreDataAfterTheEndOfTheFileAsCorrupt)
{
    std::string decoded;

    const auto result = decodeFile(fileWith(bytesFromHex(OneBlock) + "trailing data longer than a header"), collectInto(decoded));

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<CorruptData>(result.error()));
}

TEST_F(XzFileTest, TakesThePaddingTheFormatAllows)
{
    std::string decoded;

    const auto result = decodeFile(fileWith(bytesFromHex(OneBlock) + std::string(4, '\0')), collectInto(decoded));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(decoded, "hello, xz\n");
}

TEST_F(XzFileTest, ReportsPaddingOfAnotherLengthAsCorrupt)
{
    std::string decoded;

    const auto result = decodeFile(fileWith(bytesFromHex(OneBlock) + std::string(3, '\0')), collectInto(decoded));

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<CorruptData>(result.error()));
}

TEST_F(XzFileTest, StopsWhenTheConsumerAsks)
{
    std::size_t pieces = 0;

    const auto result = decodeFile(fileWith(bytesFromHex(ThreeBlocks)), [&pieces](std::string_view) {
        ++pieces;
        return false;
    });

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ConsumerStopped>(result.error()));
    EXPECT_EQ(pieces, 1U);
}

TEST(StoredXzTest, DecodesToWhatItWasGiven)
{
    const TempDirectory directory;
    for (const std::size_t size : { std::size_t{ 0 }, std::size_t{ 1 }, std::size_t{ 3 }, std::size_t{ 65536 }, std::size_t{ 65537 }, std::size_t{ 200000 } }) {
        const std::string content(size, 'x');
        const auto file = directory.writeFile("stored.xz", scrap::TestSupport::storedXz(content));

        std::string decoded;
        const auto result = decodeFile(file, [&decoded](const std::string_view piece) {
            decoded += piece;
            return true;
        });

        ASSERT_TRUE(result.has_value()) << size;
        EXPECT_EQ(decoded, content) << size;
    }
}
