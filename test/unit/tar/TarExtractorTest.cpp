#include "tar/TarExtractor.h"

#include "support/TempDirectory.h"
#include "unit/tar/TarArchiveBuilder.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <variant>
#include <vector>

using namespace scrap::Tar;
using namespace scrap::TestTar;
namespace fs = std::filesystem;

namespace {

/// A directory to extract into, inside one a test can check stays untouched.
class TarExtractorTest : public ::testing::Test {
protected:
    TarExtractorTest()
        : _destination{ _outer.makeDirectory("destination") }
    {
    }

    /// Extracts @p archive given in pieces of @p pieceSize.
    [[nodiscard]] std::expected<void, ExtractError> extract(std::string_view archive, const std::size_t pieceSize = 4096) const
    {
        auto extractor = Extractor::into(_destination);
        if (! extractor) {
            return std::unexpected(extractor.error());
        }
        for (std::size_t start = 0; start < archive.size(); start += pieceSize) {
            if (auto fed = extractor->feed(archive.substr(start, pieceSize)); ! fed) {
                return fed;
            }
        }
        return extractor->finish();
    }

    [[nodiscard]] fs::path at(std::string_view relative) const
    {
        return _destination / relative;
    }

    [[nodiscard]] std::vector<std::string> outerEntries() const
    {
        return _outer.entries();
    }

    [[nodiscard]] static mode_t permissionsOf(const fs::path& path)
    {
        struct stat status { };
        EXPECT_EQ(::lstat(path.c_str(), &status), 0) << path;
        return status.st_mode & 07777U;
    }

    template <typename Error>
    void expectRefusedWith(std::string_view archive) const
    {
        const auto result = extract(archive);
        ASSERT_FALSE(result.has_value());
        EXPECT_TRUE(std::holds_alternative<Error>(result.error()));
        EXPECT_EQ(outerEntries(), std::vector<std::string>{ "destination" });
    }

    scrap::TestSupport::TempDirectory _outer;
    fs::path _destination;
};

}  // anonymous namespace

TEST_F(TarExtractorTest, ExtractsAFileWithItsContentModeAndTime)
{
    ASSERT_TRUE(extract(fileEntry("file", "content", 0640, 1700000000) + endMarker()).has_value());

    EXPECT_EQ(_outer.readFile(at("file")), "content");
    EXPECT_EQ(permissionsOf(at("file")) & 0700U, 0600U);
    EXPECT_EQ(permissionsOf(at("file")) & 0007U, 0U);
    struct stat status { };
    ASSERT_EQ(::stat(at("file").c_str(), &status), 0);
    EXPECT_EQ(status.st_mtime, 1700000000);
}

TEST_F(TarExtractorTest, GivesEachFileItsOwnTime)
{
    ASSERT_TRUE(extract(fileEntry("first", "a", 0644, 1600000000) + fileEntry("second", "b", 0644, 1700000000) + endMarker()).has_value());

    struct stat status { };
    ASSERT_EQ(::stat(at("first").c_str(), &status), 0);
    EXPECT_EQ(status.st_mtime, 1600000000);
    ASSERT_EQ(::stat(at("second").c_str(), &status), 0);
    EXPECT_EQ(status.st_mtime, 1700000000);
}

TEST_F(TarExtractorTest, ExtractsAnEmptyFile)
{
    ASSERT_TRUE(extract(fileEntry("empty", "") + endMarker()).has_value());

    EXPECT_EQ(fs::file_size(at("empty")), 0U);
}

TEST_F(TarExtractorTest, ExtractsTheSameHoweverTheArchiveIsSplit)
{
    const std::string content(70000, 'a');
    const std::string archive = fileEntry("large", content) + fileEntry("small", "b") + endMarker();

    ASSERT_TRUE(extract(archive, 777).has_value());

    EXPECT_EQ(_outer.readFile(at("large")), content);
    EXPECT_EQ(_outer.readFile(at("small")), "b");
}

TEST_F(TarExtractorTest, KeepsAReadOnlyFileReadOnly)
{
    ASSERT_TRUE(extract(fileEntry("readonly", "content", 0444) + endMarker()).has_value());

    EXPECT_EQ(_outer.readFile(at("readonly")), "content");
    EXPECT_EQ(permissionsOf(at("readonly")) & 0200U, 0U);
}

TEST_F(TarExtractorTest, DropsSpecialPermissionBits)
{
    ASSERT_TRUE(extract(fileEntry("setuid", "", 06755) + directoryEntry("sticky/", 01777) + endMarker()).has_value());

    EXPECT_EQ(permissionsOf(at("setuid")) & 07000U, 0U);
    EXPECT_EQ(permissionsOf(at("sticky")) & 07000U, 0U);
}

TEST_F(TarExtractorTest, CreatesDirectoriesWithAndWithoutEntries)
{
    ASSERT_TRUE(extract(directoryEntry("named/") + fileEntry("named/file", "a") + fileEntry("implied/deep/file", "b") + endMarker()).has_value());

    EXPECT_TRUE(fs::is_directory(at("named")));
    EXPECT_EQ(_outer.readFile(at("named/file")), "a");
    EXPECT_EQ(_outer.readFile(at("implied/deep/file")), "b");
}

TEST_F(TarExtractorTest, GivesAnImpliedDirectoryWhatUmaskLeaves)
{
    const fs::path plain = _outer.makeDirectory("plain");
    ASSERT_TRUE(extract(fileEntry("implied/file", "a") + endMarker()).has_value());

    EXPECT_EQ(permissionsOf(at("implied")), permissionsOf(plain));
}

TEST_F(TarExtractorTest, TakesADirectoryEntryForOneAlreadyThere)
{
    const fs::path plain = _outer.makeDirectory("plain");

    ASSERT_TRUE(extract(fileEntry("dir/file", "a") + directoryEntry("dir/", 0700) + endMarker()).has_value());

    EXPECT_EQ(permissionsOf(at("dir")), permissionsOf(plain));
}

TEST_F(TarExtractorTest, KeepsAReadOnlyDirectoryUsableByItsOwner)
{
    ASSERT_TRUE(extract(directoryEntry("readonly/", 0555) + fileEntry("readonly/file", "a") + endMarker()).has_value());

    EXPECT_EQ(permissionsOf(at("readonly")) & 0700U, 0700U);
    EXPECT_EQ(_outer.readFile(at("readonly/file")), "a");
}

TEST_F(TarExtractorTest, TakesPathsStartingWithADot)
{
    ASSERT_TRUE(extract(directoryEntry("./") + directoryEntry("./dir/") + fileEntry("./dir/file", "a") + hardLinkEntry("./hard", "./dir/file") + endMarker()).has_value());

    EXPECT_EQ(_outer.readFile(at("dir/file")), "a");
    EXPECT_EQ(_outer.readFile(at("hard")), "a");
}

TEST_F(TarExtractorTest, CreatesASymbolicLink)
{
    ASSERT_TRUE(extract(fileEntry("lib/real", "a") + symbolicLinkEntry("lib/link", "real") + symbolicLinkEntry("bin/up", "../lib/real") + endMarker()).has_value());

    EXPECT_EQ(fs::read_symlink(at("lib/link")), "real");
    EXPECT_EQ(_outer.readFile(at("bin/up")), "a");
}

TEST_F(TarExtractorTest, CreatesASymbolicLinkWhoseTargetComesLater)
{
    ASSERT_TRUE(extract(symbolicLinkEntry("link", "later") + fileEntry("later", "a") + endMarker()).has_value());

    EXPECT_EQ(_outer.readFile(at("link")), "a");
}

TEST_F(TarExtractorTest, CreatesAHardLink)
{
    ASSERT_TRUE(extract(fileEntry("dir/real", "a") + hardLinkEntry("other/hard", "dir/real") + endMarker()).has_value());

    EXPECT_TRUE(fs::equivalent(at("dir/real"), at("other/hard")));
    EXPECT_EQ(fs::hard_link_count(at("dir/real")), 2U);
}

TEST_F(TarExtractorTest, RefusesAnAbsolutePath)
{
    const fs::path outside = _outer.path() / "outside";

    expectRefusedWith<UnsafeMemberPath>(fileEntry(outside.string(), "evil") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAPathGoingUp)
{
    expectRefusedWith<UnsafeMemberPath>(fileEntry("../outside", "evil") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAPathGoingUpInADirectoryEntry)
{
    expectRefusedWith<UnsafeMemberPath>(directoryEntry("../outside/") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAPathGoingUpInALongName)
{
    expectRefusedWith<UnsafeMemberPath>(gnuLongEntry('L', "../outside") + fileEntry("cut", "evil") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAPathGoingUpInAPaxHeader)
{
    expectRefusedWith<UnsafeMemberPath>(paxEntry(paxRecord("path", "../outside")) + fileEntry("cut", "evil") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAFileNamedAsTheRoot)
{
    expectRefusedWith<UnsafeMemberPath>(fileEntry("./", "evil") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAnAbsoluteSymbolicLink)
{
    const fs::path outside = _outer.path() / "outside";

    expectRefusedWith<UnsafeLinkTarget>(symbolicLinkEntry("link", outside.string()) + endMarker());

    EXPECT_FALSE(fs::is_symlink(at("link")));
}

TEST_F(TarExtractorTest, RefusesASymbolicLinkGoingAboveTheRoot)
{
    expectRefusedWith<UnsafeLinkTarget>(symbolicLinkEntry("dir/link", "../../outside") + endMarker());

    EXPECT_FALSE(fs::exists(at("dir")));
}

TEST_F(TarExtractorTest, LeavesNoParentDirectoryForARefusedHardLink)
{
    expectRefusedWith<UnsafeLinkTarget>(hardLinkEntry("dir/hard", "../outside") + endMarker());

    EXPECT_FALSE(fs::exists(at("dir")));
}

TEST_F(TarExtractorTest, JudgesTheLinkTargetBeforeWalkingThroughAFile)
{
    expectRefusedWith<UnsafeLinkTarget>(fileEntry("file", "a") + symbolicLinkEntry("file/link", "/absolute") + endMarker());
}

TEST_F(TarExtractorTest, RefusesALinkChainThatWouldLeadOutside)
{
    // Written out, each link looks as if it stays inside; followed, the last one
    // reaches the parent of the root.
    expectRefusedWith<UnsafeLinkTarget>(directoryEntry("dir/") + symbolicLinkEntry("dir/up", "..") + symbolicLinkEntry("root", "dir/up") + symbolicLinkEntry("escape", "root/..") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAFileWrittenThroughASymbolicLink)
{
    expectRefusedWith<UnsafeMemberPath>(directoryEntry("dir/") + symbolicLinkEntry("link", "dir") + fileEntry("link/file", "evil") + endMarker());

    EXPECT_FALSE(fs::exists(at("dir/file")));
}

TEST_F(TarExtractorTest, RefusesADirectoryCreatedThroughASymbolicLink)
{
    expectRefusedWith<UnsafeMemberPath>(directoryEntry("dir/") + symbolicLinkEntry("link", "dir") + directoryEntry("link/sub/") + endMarker());

    EXPECT_FALSE(fs::exists(at("dir/sub")));
}

TEST_F(TarExtractorTest, RefusesAPathThroughADanglingSymbolicLink)
{
    expectRefusedWith<UnsafeMemberPath>(symbolicLinkEntry("link", "missing") + fileEntry("link/file", "evil") + endMarker());

    EXPECT_FALSE(fs::exists(at("missing")));
}

TEST_F(TarExtractorTest, RefusesAPathThroughAFile)
{
    expectRefusedWith<UnsafeMemberPath>(fileEntry("file", "a") + fileEntry("file/inside", "b") + endMarker());
}

TEST_F(TarExtractorTest, DoesNotFollowASymbolicLinkAlreadyInTheDirectory)
{
    const fs::path outsideDirectory = _outer.makeDirectory("outside");
    fs::create_directory_symlink(outsideDirectory, at("link"));

    const auto result = extract(fileEntry("link/file", "evil") + endMarker());

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<UnsafeMemberPath>(result.error()));
    EXPECT_FALSE(fs::exists(outsideDirectory / "file"));
}

TEST_F(TarExtractorTest, DoesNotWriteThroughAFinalSymbolicLink)
{
    const fs::path outsideFile = _outer.writeFile("outside", "untouched");
    fs::create_symlink(outsideFile, at("link"));

    const auto result = extract(fileEntry("link", "evil") + endMarker());

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<CannotWriteMember>(result.error()));
    EXPECT_EQ(std::get<CannotWriteMember>(result.error()).code, std::make_error_code(std::errc::file_exists));
    EXPECT_EQ(_outer.readFile(outsideFile), "untouched");
}

TEST_F(TarExtractorTest, RefusesAHardLinkGoingUp)
{
    expectRefusedWith<UnsafeLinkTarget>(hardLinkEntry("hard", "../outside") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAnAbsoluteHardLink)
{
    expectRefusedWith<UnsafeLinkTarget>(hardLinkEntry("hard", "/etc/passwd") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAHardLinkThroughASymbolicLink)
{
    expectRefusedWith<UnsafeLinkTarget>(fileEntry("dir/file", "a") + symbolicLinkEntry("link", "dir") + hardLinkEntry("hard", "link/file") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAHardLinkToASymbolicLink)
{
    expectRefusedWith<UnsafeLinkTarget>(symbolicLinkEntry("link", "file") + hardLinkEntry("hard", "link") + endMarker());
}

TEST_F(TarExtractorTest, RefusesAHardLinkToADirectory)
{
    expectRefusedWith<UnsafeLinkTarget>(directoryEntry("dir/") + hardLinkEntry("hard", "dir") + endMarker());
}

TEST_F(TarExtractorTest, ReportsAHardLinkToSomethingMissing)
{
    expectRefusedWith<CannotWriteMember>(hardLinkEntry("hard", "missing") + endMarker());
}

TEST_F(TarExtractorTest, ReportsTwoMembersWithOnePath)
{
    const auto result = extract(fileEntry("file", "first") + fileEntry("file", "second") + endMarker());

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<CannotWriteMember>(result.error()));
    const auto& failure = std::get<CannotWriteMember>(result.error());
    EXPECT_EQ(failure.path, "file");
    EXPECT_EQ(failure.code, std::make_error_code(std::errc::file_exists));
    EXPECT_EQ(_outer.readFile(at("file")), "first");
}

TEST_F(TarExtractorTest, ReportsADirectoryInThePlaceOfAFile)
{
    expectRefusedWith<UnsafeMemberPath>(fileEntry("name", "a") + directoryEntry("name/") + endMarker());
}

TEST_F(TarExtractorTest, ReportsAFileInThePlaceOfADirectory)
{
    expectRefusedWith<CannotWriteMember>(directoryEntry("name/") + fileEntry("name", "a") + endMarker());
}

TEST_F(TarExtractorTest, ReportsAnArchiveCutShort)
{
    const std::string archive = fileEntry("file", std::string(1000, 'a'));

    const auto result = extract(std::string_view{ archive }.substr(0, 700));

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ReadError>(result.error()));
    EXPECT_TRUE(std::holds_alternative<TruncatedArchive>(std::get<ReadError>(result.error())));
}

TEST_F(TarExtractorTest, ReportsADirectoryThatDoesNotExist)
{
    const auto extractor = Extractor::into(at("missing"));

    ASSERT_FALSE(extractor.has_value());
    ASSERT_TRUE(std::holds_alternative<CannotOpenDirectory>(extractor.error()));
    const auto& failure = std::get<CannotOpenDirectory>(extractor.error());
    EXPECT_EQ(failure.path, at("missing"));
    EXPECT_EQ(failure.code, std::make_error_code(std::errc::no_such_file_or_directory));
}
