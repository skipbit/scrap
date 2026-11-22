#include "project/DirectoryWalk.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

using namespace scrap::Project;
using scrap::TestSupport::TempDirectory;

namespace {

using Paths = std::vector<std::filesystem::path>;

}  // namespace

/**
 * Every entry below the directory is visited with its path relative to the
 * project root, normalized.
 */
TEST(DirectoryWalkTest, VisitsEntriesWithRootRelativePaths)
{
    const TempDirectory temp;
    temp.writeFile("src/a.cpp", "");
    temp.writeFile("src/net/b.cpp", "");

    Paths visited;
    const auto walked = walkDirectory(temp.path(), "./src", [&](const std::filesystem::directory_entry&, const std::filesystem::path& relative) {
        visited.push_back(relative);
        return WalkStep::Continue;
    });
    std::ranges::sort(visited);

    ASSERT_TRUE(walked.has_value());
    EXPECT_EQ(visited, (Paths{ "src/a.cpp", "src/net", "src/net/b.cpp" }));
}

/**
 * An entry the visitor skips is visited, and nothing below it is.
 */
TEST(DirectoryWalkTest, SkipBelowKeepsTheWalkOutOfADirectory)
{
    const TempDirectory temp;
    temp.writeFile("src/a.cpp", "");
    temp.writeFile("src/skipped/b.cpp", "");

    Paths visited;
    const auto walked = walkDirectory(temp.path(), "src", [&](const std::filesystem::directory_entry&, const std::filesystem::path& relative) {
        visited.push_back(relative);
        return relative.filename() == "skipped" ? WalkStep::SkipBelow : WalkStep::Continue;
    });
    std::ranges::sort(visited);

    ASSERT_TRUE(walked.has_value());
    EXPECT_EQ(visited, (Paths{ "src/a.cpp", "src/skipped" }));
}

/**
 * A directory that does not exist holds nothing, which is not a failure.
 */
TEST(DirectoryWalkTest, VisitsNothingInAMissingDirectory)
{
    const TempDirectory temp;

    int visits = 0;
    const auto walked = walkDirectory(temp.path(), "src", [&](const std::filesystem::directory_entry&, const std::filesystem::path&) {
        ++visits;
        return WalkStep::Continue;
    });

    EXPECT_TRUE(walked.has_value());
    EXPECT_EQ(visits, 0);
}

/**
 * A directory that exists and cannot be read is reported with its path.
 */
TEST(DirectoryWalkTest, ReportsADirectoryItCannotRead)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", "");
    const std::filesystem::path locked = temp.makeDirectory("src/locked");
    temp.writeFile("src/locked/hidden.cpp", "");

    std::error_code ec;
    std::filesystem::permissions(locked, std::filesystem::perms::none, ec);
    ASSERT_FALSE(ec) << ec.message();

    // A user who reads the directory anyway, root among them, cannot observe
    // the failure; the fixture is restored before skipping so it can be removed.
    const std::filesystem::directory_iterator probe(locked, ec);
    if (! ec) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);
        GTEST_SKIP() << "this user reads a directory with no permissions";
    }

    const auto walked = walkDirectory(temp.path(), "src", [](const std::filesystem::directory_entry&, const std::filesystem::path&) {
        return WalkStep::Continue;
    });

    std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);

    ASSERT_FALSE(walked.has_value());
    EXPECT_EQ(walked.error().directory, locked);
    EXPECT_FALSE(walked.error().reason.empty());
}
