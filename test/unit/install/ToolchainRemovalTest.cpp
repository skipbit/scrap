#include "install/ToolchainRemoval.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

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
