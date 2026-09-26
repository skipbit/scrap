#include "build/BuildOutput.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <system_error>

using namespace scrap::Build;
using scrap::TestSupport::TempDirectory;

/**
 * A directory is removed with everything below it.
 */
TEST(BuildOutputTest, RemovesTheDirectoryWithWhatItHolds)
{
    const TempDirectory temp;
    temp.writeFile("build/debug/bin/app", "binary");
    temp.writeFile("build/other/notes.txt", "kept by someone else");

    const auto removed = removeOutput(temp.path() / "build");

    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, OutputRemoval::Removed);
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "build"));
}

/**
 * A path with nothing at it is not a failure.
 */
TEST(BuildOutputTest, ReportsThatNothingIsThere)
{
    const TempDirectory temp;

    const auto removed = removeOutput(temp.path() / "build");

    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, OutputRemoval::NothingThere);
}

/**
 * A link at the path is removed, and what it points to is left alone.
 */
TEST(BuildOutputTest, RemovesALinkWithoutFollowingIt)
{
    const TempDirectory temp;
    temp.writeFile("elsewhere/kept.txt", "kept");
    std::filesystem::create_directory_symlink(temp.path() / "elsewhere", temp.path() / "build");

    const auto removed = removeOutput(temp.path() / "build");

    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, OutputRemoval::Removed);
    std::error_code ec;
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::symlink_status(temp.path() / "build", ec)));
    EXPECT_TRUE(std::filesystem::is_regular_file(temp.path() / "elsewhere" / "kept.txt"));
}

/**
 * A link below the directory is removed, and what it points to is left alone.
 */
TEST(BuildOutputTest, LeavesWhatALinkInsidePointsTo)
{
    const TempDirectory temp;
    temp.writeFile("elsewhere/kept.txt", "kept");
    temp.makeDirectory("build");
    std::filesystem::create_directory_symlink(temp.path() / "elsewhere", temp.path() / "build" / "link");

    const auto removed = removeOutput(temp.path() / "build");

    ASSERT_TRUE(removed.has_value());
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "build"));
    EXPECT_TRUE(std::filesystem::is_regular_file(temp.path() / "elsewhere" / "kept.txt"));
}

/**
 * A regular file at the path was not written by a build, so it is kept.
 */
TEST(BuildOutputTest, RefusesAFileThatIsNotADirectory)
{
    const TempDirectory temp;
    temp.writeFile("build", "a file of the user's");

    const auto removed = removeOutput(temp.path() / "build");

    ASSERT_FALSE(removed.has_value());
    EXPECT_EQ(removed.error().problem, OutputRemovalProblem::NotADirectory);
    EXPECT_EQ(removed.error().path, temp.path() / "build");
    EXPECT_EQ(temp.readFile("build"), "a file of the user's");
}

/**
 * A directory whose entries cannot be removed is reported with the reason.
 */
TEST(BuildOutputTest, ReportsWhatItCannotRemove)
{
    const TempDirectory temp;
    temp.writeFile("build/locked/app", "binary");
    const std::filesystem::path locked = temp.path() / "build" / "locked";

    std::error_code ec;
    std::filesystem::permissions(locked, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec, ec);
    ASSERT_FALSE(ec) << ec.message();
    // A user who writes the directory anyway, root among them, cannot observe
    // the failure; the fixture is restored before skipping so it can be removed.
    if (std::ofstream{ locked / "probe" }) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);
        GTEST_SKIP() << "this user writes a directory with no write permission";
    }

    const auto removed = removeOutput(temp.path() / "build");

    std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);

    ASSERT_FALSE(removed.has_value());
    EXPECT_EQ(removed.error().problem, OutputRemovalProblem::CannotRemove);
    EXPECT_EQ(removed.error().path, temp.path() / "build");
    EXPECT_TRUE(removed.error().code);
}
