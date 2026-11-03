#include "tar/TarReader.h"

#include "support/TarArchiveBuilder.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace scrap::Tar;
using namespace scrap::TestTar;

namespace {

/// What reading an archive produced.
struct ReadResult {
    std::vector<Entry> entries;
    std::vector<std::string> contents;
    bool ended = false;
    std::optional<ReadError> error;
};

/// Reads @p archive given in pieces of @p pieceSize, ending the input after.
ReadResult readAll(std::string_view archive, const std::size_t pieceSize = 512)
{
    ReadResult result;
    Reader reader;
    std::size_t position = 0;
    while (true) {
        auto event = reader.next();
        if (! event) {
            result.error = event.error();
            return result;
        }
        if (std::holds_alternative<EndOfArchive>(*event)) {
            result.ended = true;
            return result;
        }
        if (auto* entry = std::get_if<Entry>(&*event)) {
            result.entries.push_back(*entry);
            result.contents.emplace_back();
        } else if (const auto* data = std::get_if<EntryData>(&*event)) {
            result.contents.back().append(data->bytes);
        } else if (position >= archive.size()) {
            reader.endInput();
        } else {
            reader.give(archive.substr(position, pieceSize));
            position += pieceSize;
        }
    }
}

template <typename Error>
bool failedWith(const ReadResult& result)
{
    return result.error && std::holds_alternative<Error>(*result.error);
}

}  // anonymous namespace

TEST(TarReaderTest, ReadsAnEmptyArchive)
{
    const auto result = readAll(endMarker());

    EXPECT_FALSE(result.error);
    EXPECT_TRUE(result.ended);
    EXPECT_TRUE(result.entries.empty());
}

TEST(TarReaderTest, ReadsAFile)
{
    const auto result = readAll(fileEntry("dir/file", "content", 0640, 1700000000) + endMarker());

    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].path, "dir/file");
    EXPECT_EQ(result.entries[0].type, EntryType::File);
    EXPECT_EQ(result.entries[0].size, 7U);
    EXPECT_EQ(result.entries[0].mode, 0640U);
    EXPECT_EQ(result.entries[0].modificationTime, 1700000000);
    EXPECT_EQ(result.contents[0], "content");
    EXPECT_TRUE(result.ended);
}

TEST(TarReaderTest, ReadsTheOldTypeOfAFile)
{
    const auto result = readAll(headerBlock({ .name = "old", .type = '\0' }) + endMarker());

    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].type, EntryType::File);
}

TEST(TarReaderTest, ReadsDirectoriesAndLinks)
{
    const auto result = readAll(directoryEntry("dir/") + symbolicLinkEntry("dir/soft", "file") + hardLinkEntry("hard", "dir/file") + endMarker());

    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.entries.size(), 3U);
    EXPECT_EQ(result.entries[0].type, EntryType::Directory);
    EXPECT_EQ(result.entries[1].type, EntryType::SymbolicLink);
    EXPECT_EQ(result.entries[1].linkTarget, "file");
    EXPECT_EQ(result.entries[2].type, EntryType::HardLink);
    EXPECT_EQ(result.entries[2].linkTarget, "dir/file");
}

TEST(TarReaderTest, ReadsTheSameHoweverTheInputIsSplit)
{
    const std::string longName(200, 'n');
    const std::string archive = fileEntry("first", std::string(1000, 'a')) + gnuLongEntry('L', longName) + fileEntry("cut", "second") + paxEntry(paxRecord("path", "from-pax")) + fileEntry("cut", std::string(512, 'c')) + endMarker();

    for (const std::size_t pieceSize : { std::size_t{ 1 }, std::size_t{ 7 }, std::size_t{ 511 }, std::size_t{ 513 }, archive.size() }) {
        const auto result = readAll(archive, pieceSize);

        ASSERT_FALSE(result.error) << "pieces of " << pieceSize;
        ASSERT_EQ(result.entries.size(), 3U) << "pieces of " << pieceSize;
        EXPECT_EQ(result.entries[1].path, longName);
        EXPECT_EQ(result.entries[2].path, "from-pax");
        EXPECT_EQ(result.contents[0], std::string(1000, 'a'));
        EXPECT_EQ(result.contents[1], "second");
        EXPECT_EQ(result.contents[2], std::string(512, 'c'));
    }
}

TEST(TarReaderTest, JoinsTheUstarPrefixAndName)
{
    const auto result = readAll(headerBlock({ .name = "name", .prefix = "some/prefix" }) + endMarker());

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].path, "some/prefix/name");
}

TEST(TarReaderTest, IgnoresThePrefixBytesInTheGnuFormat)
{
    // The GNU format keeps access and change times where ustar has the prefix.
    std::string block = headerBlock({ .name = "name", .gnu = true });
    block[345] = '1';

    const auto result = readAll(withChecksum(block) + endMarker());

    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].path, "name");
}

TEST(TarReaderTest, ReadsAGnuLongNameAndLongLinkTarget)
{
    const std::string longName(300, 'n');
    const std::string longTarget(300, 't');

    const auto result = readAll(gnuLongEntry('L', longName) + gnuLongEntry('K', longTarget) + headerBlock({ .name = "cut", .type = '2', .linkName = "cut", .gnu = true }) + fileEntry("after", "") + endMarker());

    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.entries.size(), 2U);
    EXPECT_EQ(result.entries[0].path, longName);
    EXPECT_EQ(result.entries[0].linkTarget, longTarget);
    EXPECT_EQ(result.entries[1].path, "after");
    EXPECT_TRUE(result.entries[1].linkTarget.empty());
}

TEST(TarReaderTest, ReadsAPaxExtendedHeader)
{
    const std::string records = paxRecord("path", "long/path") + paxRecord("linkpath", "long/target") + paxRecord("mtime", "1700000000.25") + paxRecord("atime", "ignored") + paxRecord("size", "5");

    const auto result = readAll(paxEntry(records) + headerBlock({ .name = "cut" }) + padded("sized") + fileEntry("after", "") + endMarker());

    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.entries.size(), 2U);
    EXPECT_EQ(result.entries[0].path, "long/path");
    EXPECT_EQ(result.entries[0].linkTarget, "long/target");
    EXPECT_EQ(result.entries[0].size, 5U);
    EXPECT_EQ(result.entries[0].modificationTime, 1700000000);
    EXPECT_EQ(result.contents[0], "sized");
    EXPECT_EQ(result.entries[1].path, "after");
    EXPECT_EQ(result.entries[1].size, 0U);
}

TEST(TarReaderTest, SkipsAGlobalPaxHeader)
{
    const auto result = readAll(paxEntry(paxRecord("path", "ignored"), 'g') + fileEntry("file", "") + endMarker());

    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].path, "file");
    EXPECT_TRUE(result.entries[0].linkTarget.empty());
}

TEST(TarReaderTest, ReadsASizeInBaseTwoFiftySix)
{
    // The high bit of the first byte marks the encoding; the number follows.
    std::string block = headerBlock({ .name = "big" });
    std::fill(block.begin() + 124, block.begin() + 136, '\0');
    block[124] = static_cast<char>(0x80);
    block[135] = 3;

    const auto result = readAll(withChecksum(block) + padded("abc") + endMarker());

    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].size, 3U);
    EXPECT_EQ(result.contents[0], "abc");
}

TEST(TarReaderTest, ReportsANegativeBaseTwoFiftySixNumber)
{
    // Minus one in the mode, a field short enough for the bytes to fit a number.
    std::string block = headerBlock({ .name = "negative" });
    std::fill(block.begin() + 100, block.begin() + 108, static_cast<char>(0xff));

    EXPECT_TRUE(failedWith<InvalidHeader>(readAll(withChecksum(block) + endMarker())));
}

TEST(TarReaderTest, ReportsABaseTwoFiftySixNumberTooLargeToHold)
{
    std::string block = headerBlock({ .name = "huge" });
    std::fill(block.begin() + 124, block.begin() + 136, static_cast<char>(0x3f));
    block[124] = static_cast<char>(0x80);

    EXPECT_TRUE(failedWith<InvalidHeader>(readAll(withChecksum(block) + endMarker())));
}

TEST(TarReaderTest, ReportsANonOctalField)
{
    std::string block = headerBlock({ .name = "file" });
    block[100] = '9';

    EXPECT_TRUE(failedWith<InvalidHeader>(readAll(withChecksum(block) + endMarker())));
}

TEST(TarReaderTest, ReportsABadChecksum)
{
    std::string block = headerBlock({ .name = "file" });
    block[0] = 'g';

    const auto result = readAll(block + endMarker());

    ASSERT_TRUE(failedWith<InvalidHeader>(result));
    EXPECT_EQ(std::get<InvalidHeader>(*result.error).offset, 0U);
}

TEST(TarReaderTest, ReportsTheOffsetOfABadHeader)
{
    std::string block = headerBlock({ .name = "second" });
    block[0] = 'g';

    const auto result = readAll(fileEntry("first", "content") + block);

    ASSERT_TRUE(failedWith<InvalidHeader>(result));
    EXPECT_EQ(std::get<InvalidHeader>(*result.error).offset, 1024U);
}

TEST(TarReaderTest, ReportsAnArchiveOfNeitherFormat)
{
    std::string block = headerBlock({ .name = "file" });
    block[257] = 'x';

    EXPECT_TRUE(failedWith<InvalidHeader>(readAll(withChecksum(block) + endMarker())));
}

TEST(TarReaderTest, ReportsAnInputThatIsNotAnArchive)
{
    const auto result = readAll(std::string(2048, 'x'));

    EXPECT_TRUE(failedWith<InvalidHeader>(result));
}

TEST(TarReaderTest, ReportsAnEmptyInputAsTruncated)
{
    EXPECT_TRUE(failedWith<TruncatedArchive>(readAll("")));
}

TEST(TarReaderTest, ReportsAnInputCutInsideAHeader)
{
    EXPECT_TRUE(failedWith<TruncatedArchive>(readAll(headerBlock({ .name = "file" }).substr(0, 100))));
}

TEST(TarReaderTest, ReportsAnInputCutInsideContent)
{
    const std::string archive = fileEntry("file", std::string(1000, 'a'));

    EXPECT_TRUE(failedWith<TruncatedArchive>(readAll(archive.substr(0, 700))));
}

TEST(TarReaderTest, ReportsAnInputCutInsidePadding)
{
    const std::string archive = fileEntry("file", "abc");

    EXPECT_TRUE(failedWith<TruncatedArchive>(readAll(archive.substr(0, 600))));
}

TEST(TarReaderTest, ReportsAnInputWithoutAnEndMarker)
{
    EXPECT_TRUE(failedWith<TruncatedArchive>(readAll(fileEntry("file", "abc"))));
}

TEST(TarReaderTest, ReportsAnInputWithHalfAnEndMarker)
{
    EXPECT_TRUE(failedWith<TruncatedArchive>(readAll(fileEntry("file", "abc") + std::string(512, '\0'))));
}

TEST(TarReaderTest, ReportsAHeaderAfterOneZeroBlock)
{
    EXPECT_TRUE(failedWith<InvalidHeader>(readAll(std::string(512, '\0') + fileEntry("file", "abc") + endMarker())));
}

TEST(TarReaderTest, ReportsAnExtendedHeaderWithNoEntryAfterIt)
{
    EXPECT_TRUE(failedWith<InvalidHeader>(readAll(paxEntry(paxRecord("path", "dangling")) + endMarker())));
}

TEST(TarReaderTest, ReportsALongNameWithNoEntryAfterIt)
{
    EXPECT_TRUE(failedWith<InvalidHeader>(readAll(gnuLongEntry('L', "dangling") + endMarker())));
}

TEST(TarReaderTest, IgnoresWhatFollowsTheEndMarker)
{
    const auto result = readAll(fileEntry("file", "abc") + endMarker() + "anything at all");

    EXPECT_FALSE(result.error);
    EXPECT_TRUE(result.ended);
    EXPECT_EQ(result.entries.size(), 1U);
}

TEST(TarReaderTest, ReportsEntriesOfOtherKinds)
{
    for (const char type : { '3', '4', '6', 'S', 'D', 'V', 'M', 'Z' }) {
        const auto result = readAll(headerBlock({ .name = "special", .type = type }) + endMarker());

        ASSERT_TRUE(failedWith<UnsupportedEntry>(result)) << "type " << type;
        EXPECT_EQ(std::get<UnsupportedEntry>(*result.error).path, "special");
        EXPECT_EQ(std::get<UnsupportedEntry>(*result.error).type, type);
    }
}

TEST(TarReaderTest, ReportsSparseFilesInAnExtendedHeader)
{
    const auto result = readAll(paxEntry(paxRecord("GNU.sparse.major", "1")) + fileEntry("sparse", "") + endMarker());

    EXPECT_TRUE(failedWith<UnsupportedEntry>(result));
}

TEST(TarReaderTest, ReportsContentOnAnEntryThatCarriesNone)
{
    for (const char type : { '1', '2', '5' }) {
        const auto result = readAll(headerBlock({ .name = "entry", .type = type, .size = 512 }) + padded(std::string(512, 'a')) + endMarker());

        EXPECT_TRUE(failedWith<InvalidHeader>(result)) << "type " << type;
    }
}

TEST(TarReaderTest, ReportsAnOversizedMetadataHeader)
{
    const auto result = readAll(headerBlock({ .name = "huge", .type = 'L', .size = (std::uint64_t{ 1 } << 20U) + 1, .gnu = true }));

    EXPECT_TRUE(failedWith<InvalidHeader>(result));
}

TEST(TarReaderTest, ReportsANulInsideALongName)
{
    const auto result = readAll(gnuLongEntry('L', std::string_view{ "safe\0../../evil", 15 }) + fileEntry("cut", "") + endMarker());

    EXPECT_TRUE(failedWith<InvalidHeader>(result));
}

TEST(TarReaderTest, ReportsANulInsideAPaxPath)
{
    const auto result = readAll(paxEntry(paxRecord("path", std::string_view{ "safe\0../../evil", 15 })) + fileEntry("cut", "") + endMarker());

    EXPECT_TRUE(failedWith<InvalidHeader>(result));
}

TEST(TarReaderTest, ReportsMalformedPaxRecords)
{
    for (const std::string_view records : { "no-length\n", "5 x=1\n", "99 path=short\n", "x path=a\n", "8 nokey\n" }) {
        const auto result = readAll(paxEntry(records) + fileEntry("cut", "") + endMarker());

        EXPECT_TRUE(failedWith<InvalidHeader>(result)) << records;
    }
}

TEST(TarReaderTest, ReportsAMalformedPaxSize)
{
    const auto result = readAll(paxEntry(paxRecord("size", "12x")) + fileEntry("cut", "") + endMarker());

    EXPECT_TRUE(failedWith<InvalidHeader>(result));
}
