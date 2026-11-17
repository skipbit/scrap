#include "install/ToolchainRemoval.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

using namespace scrap::Install;
using scrap::TestSupport::TempDirectory;

namespace {

/** Lay out a toolchain at @p directory below @p home, as an install would. */
void lay(const TempDirectory& home, const std::string& directory)
{
    home.writeFile(directory + "/bin/g++", "");
    home.writeFile(directory + "/lib/libstdc++.so", "");
}

}  // anonymous namespace

/**
 * The toolchain is taken away with its version directory, and the other
 * versions of the family stay, with nothing left below the work directory.
 */
TEST(ToolchainRemovalTest, RemovesOneVersionAndLeavesTheOthers)
{
    const TempDirectory home;
    lay(home, "toolchains/gcc/15.2.0/linux-x86_64");
    lay(home, "toolchains/gcc/16.2.0/linux-x86_64");

    const auto removed = removeInstalledToolchain(home.path(), home.path() / "toolchains/gcc/15.2.0/linux-x86_64");

    ASSERT_TRUE(removed.has_value()) << removed.error().code.message();
    EXPECT_FALSE(removed->leftAt.has_value());
    EXPECT_FALSE(std::filesystem::exists(home.path() / "toolchains/gcc/15.2.0"));
    EXPECT_TRUE(std::filesystem::exists(home.path() / "toolchains/gcc/16.2.0/linux-x86_64/bin/g++"));
    EXPECT_TRUE(std::filesystem::is_empty(home.path() / "toolchains/.work"));
}

/**
 * When the last version of a family goes, the family goes too, and the
 * toolchains directory stays.
 */
TEST(ToolchainRemovalTest, RemovesTheFamilyWithItsLastVersion)
{
    const TempDirectory home;
    lay(home, "toolchains/gcc/15.2.0/linux-x86_64");

    ASSERT_TRUE(removeInstalledToolchain(home.path(), home.path() / "toolchains/gcc/15.2.0/linux-x86_64").has_value());

    EXPECT_FALSE(std::filesystem::exists(home.path() / "toolchains/gcc"));
    EXPECT_TRUE(std::filesystem::exists(home.path() / "toolchains"));
}

/**
 * The same version installed for another host stays.
 */
TEST(ToolchainRemovalTest, LeavesTheVersionForAnotherHost)
{
    const TempDirectory home;
    lay(home, "toolchains/gcc/15.2.0/linux-x86_64");
    lay(home, "toolchains/gcc/15.2.0/darwin-aarch64");

    ASSERT_TRUE(removeInstalledToolchain(home.path(), home.path() / "toolchains/gcc/15.2.0/linux-x86_64").has_value());

    EXPECT_FALSE(std::filesystem::exists(home.path() / "toolchains/gcc/15.2.0/linux-x86_64"));
    EXPECT_TRUE(std::filesystem::exists(home.path() / "toolchains/gcc/15.2.0/darwin-aarch64/bin/g++"));
}

/**
 * A directory that cannot be moved away is reported with why, and nothing
 * is taken away.
 */
TEST(ToolchainRemovalTest, ReportsWhatCannotBeMovedAway)
{
    const TempDirectory home;
    lay(home, "toolchains/gcc/16.2.0/linux-x86_64");
    const std::filesystem::path missing = home.path() / "toolchains/gcc/15.2.0/linux-x86_64";

    const auto removed = removeInstalledToolchain(home.path(), missing);

    ASSERT_FALSE(removed.has_value());
    EXPECT_EQ(removed.error().path, missing);
    EXPECT_TRUE(removed.error().code);
    EXPECT_TRUE(std::filesystem::exists(home.path() / "toolchains/gcc/16.2.0/linux-x86_64/bin/g++"));
}

/**
 * A work directory that cannot be made is told as why the toolchain was not
 * taken away, and the toolchain stays.
 */
TEST(ToolchainRemovalTest, ReportsAWorkDirectoryThatCannotBeMade)
{
    const TempDirectory home;
    lay(home, "toolchains/gcc/15.2.0/linux-x86_64");
    home.writeFile("toolchains/.work", "in the way");
    const std::filesystem::path directory = home.path() / "toolchains/gcc/15.2.0/linux-x86_64";

    const auto removed = removeInstalledToolchain(home.path(), directory);

    ASSERT_FALSE(removed.has_value());
    EXPECT_EQ(removed.error().path, directory);
    EXPECT_EQ(removed.error().workDirectory, std::optional<std::filesystem::path>{ home.path() / "toolchains/.work" });
    EXPECT_TRUE(removed.error().code);
    EXPECT_TRUE(std::filesystem::exists(directory / "bin/g++"));
}

/**
 * What cannot be deleted once the toolchain is moved away is left below the
 * work directory, and where is told; the toolchain is gone from its place.
 */
TEST(ToolchainRemovalTest, TellsWhereWhatCannotBeDeletedIsLeft)
{
    const TempDirectory home;
    lay(home, "toolchains/gcc/15.2.0/linux-x86_64");
    const std::filesystem::path locked = home.path() / "toolchains/gcc/15.2.0/linux-x86_64/lib";
    std::error_code ec;
    std::filesystem::permissions(locked, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec, ec);
    ASSERT_FALSE(ec) << ec.message();
    // A user who writes the directory anyway, root among them, cannot observe
    // the failure; the fixture is restored before skipping so it can be removed.
    if (std::ofstream{ locked / "probe" }) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);
        GTEST_SKIP() << "this user writes a directory with no write permission";
    }

    const auto removed = removeInstalledToolchain(home.path(), home.path() / "toolchains/gcc/15.2.0/linux-x86_64");

    ASSERT_TRUE(removed.has_value()) << removed.error().code.message();
    ASSERT_TRUE(removed->leftAt.has_value());
    EXPECT_EQ(removed->leftAt->parent_path(), home.path() / "toolchains/.work");
    EXPECT_FALSE(std::filesystem::exists(home.path() / "toolchains/gcc/15.2.0"));
    EXPECT_TRUE(std::filesystem::exists(*removed->leftAt / "removed/lib/libstdc++.so"));
    std::filesystem::permissions(*removed->leftAt / "removed/lib", std::filesystem::perms::owner_all, ec);
    EXPECT_FALSE(ec) << ec.message();
}

/**
 * What earlier runs left in the work directory a day ago or more is
 * discarded, and what is younger stays.
 */
TEST(ToolchainRemovalTest, DiscardsWhatEarlierRunsLeftADayAgo)
{
    const TempDirectory home;
    lay(home, "toolchains/gcc/15.2.0/linux-x86_64");
    const auto now = std::filesystem::file_time_type::clock::now();
    const std::filesystem::path old = home.makeDirectory("toolchains/.work/uninstall-1-0");
    const std::filesystem::path young = home.makeDirectory("toolchains/.work/uninstall-2-0");
    std::filesystem::last_write_time(old, now - std::chrono::hours{ 25 });
    std::filesystem::last_write_time(young, now - std::chrono::hours{ 1 });

    ASSERT_TRUE(removeInstalledToolchain(home.path(), home.path() / "toolchains/gcc/15.2.0/linux-x86_64").has_value());

    EXPECT_FALSE(std::filesystem::exists(old));
    EXPECT_TRUE(std::filesystem::exists(young));
}
