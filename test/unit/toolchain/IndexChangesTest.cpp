#include "toolchain/IndexChanges.h"

#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace scrap::Toolchain;

namespace {

ToolchainVersion version(std::string_view text)
{
    return ToolchainVersion::parse(text).value();
}

InstallingHost linuxHost()
{
    return { .environment = { .platform = "linux", .arch = "x86_64" }, .scrapVersion = version("1.0.0"), .glibcVersion = version("2.31") };
}

Ingot ingotFor(std::string_view platform)
{
    return { .platform = std::string{ platform }, .arch = "x86_64", .source = UrlSource{ .url = "https://example.com/a", .sha256 = "abc" }, .glibcVersion = std::nullopt };
}

IndexedToolchain toolchain(std::string_view family, std::string_view text, const ToolchainStatus status = ToolchainStatus::Active, std::string_view platform = "linux")
{
    return { .family = std::string{ family },
             .version = version(text),
             .status = status,
             .compiler = "g++",
             .cCompiler = "gcc",
             .minScrapVersion = std::nullopt,
             .ingots = { ingotFor(platform) } };
}

/**
 * A change of status as a test reads it.
 */
std::string statusChange(std::string_view name, ToolchainStatus before, ToolchainStatus after)
{
    return std::string{ name } + " " + std::to_string(static_cast<int>(before)) + " -> " + std::to_string(static_cast<int>(after));
}

std::vector<std::string> changedOf(const IndexChanges& changes)
{
    std::vector<std::string> lines;
    for (const ChangedToolchain& each : changes.changed) {
        lines.push_back(statusChange(each.name, each.before, each.after));
    }
    return lines;
}

}  // anonymous namespace

/**
 * New versions are told by family, then by version as numbers.
 */
TEST(IndexChangesTest, TellsTheVersionsThatAreNew)
{
    const ToolchainIndex before{ .toolchains = { toolchain("gcc", "15.9.0") } };
    const ToolchainIndex after{ .toolchains = { toolchain("llvm", "22.1.0"), toolchain("gcc", "15.10.0"), toolchain("gcc", "15.9.0") } };

    const auto changes = compareIndexes(before, after, linuxHost());

    EXPECT_EQ(changes.added, (std::vector<std::string>{ "gcc@15.10.0", "llvm@22.1.0" }));
    EXPECT_TRUE(changes.changed.empty());
}

TEST(IndexChangesTest, TellsTheStatusesThatChangedTheDisabledAmongThem)
{
    const ToolchainIndex before{ .toolchains = { toolchain("gcc", "15.1.0"), toolchain("gcc", "15.2.0"), toolchain("gcc", "16.1.0") } };
    const ToolchainIndex after{ .toolchains = { toolchain("gcc", "15.1.0", ToolchainStatus::Disabled), toolchain("gcc", "15.2.0", ToolchainStatus::Deprecated), toolchain("gcc", "16.1.0") } };

    const auto changes = compareIndexes(before, after, linuxHost());

    EXPECT_TRUE(changes.added.empty());
    EXPECT_EQ(changedOf(changes),
              (std::vector<std::string>{ statusChange("gcc@15.1.0", ToolchainStatus::Active, ToolchainStatus::Disabled),
                                         statusChange("gcc@15.2.0", ToolchainStatus::Active, ToolchainStatus::Deprecated) }));
}

/**
 * Offered again, a version is told once, by the change of its status.
 */
TEST(IndexChangesTest, TellsAVersionOfferedAgainAsChanged)
{
    const ToolchainIndex before{ .toolchains = { toolchain("gcc", "15.1.0", ToolchainStatus::Disabled) } };
    const ToolchainIndex after{ .toolchains = { toolchain("gcc", "15.1.0") } };

    const auto changes = compareIndexes(before, after, linuxHost());

    EXPECT_TRUE(changes.added.empty());
    EXPECT_EQ(changedOf(changes), (std::vector<std::string>{ statusChange("gcc@15.1.0", ToolchainStatus::Disabled, ToolchainStatus::Active) }));
}

TEST(IndexChangesTest, TellsAVersionThatGainedAnIngotForTheHostAsNew)
{
    const ToolchainIndex before{ .toolchains = { toolchain("gcc", "15.1.0", ToolchainStatus::Active, "darwin") } };
    const ToolchainIndex after{ .toolchains = { toolchain("gcc", "15.1.0") } };

    const auto changes = compareIndexes(before, after, linuxHost());

    EXPECT_EQ(changes.added, (std::vector<std::string>{ "gcc@15.1.0" }));
    EXPECT_TRUE(changes.changed.empty());
}

/**
 * What is not for the host, or cannot be installed, is not told: a new
 * version that is disabled or needs a newer glibc, a status that changed on
 * another host's ingot, and a version that is gone.
 */
TEST(IndexChangesTest, TellsNothingTheHostCannotInstall)
{
    IndexedToolchain needsNewerGlibc = toolchain("gcc", "16.2.0");
    needsNewerGlibc.ingots.front().glibcVersion = version("2.35");
    const ToolchainIndex before{ .toolchains = { toolchain("gcc", "15.1.0", ToolchainStatus::Active, "darwin"), toolchain("gcc", "15.2.0") } };
    const ToolchainIndex after{ .toolchains = { toolchain("gcc", "15.1.0", ToolchainStatus::Deprecated, "darwin"),
                                                toolchain("gcc", "16.1.0", ToolchainStatus::Disabled),
                                                needsNewerGlibc } };

    const auto changes = compareIndexes(before, after, linuxHost());

    EXPECT_TRUE(changes.added.empty());
    EXPECT_TRUE(changes.changed.empty());
}

TEST(IndexChangesTest, CountsTheToolchainsTheHostCanInstall)
{
    const ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.1.0"),
                                                toolchain("gcc", "15.2.0", ToolchainStatus::Deprecated),
                                                toolchain("gcc", "15.3.0", ToolchainStatus::Disabled),
                                                toolchain("llvm", "22.1.0", ToolchainStatus::Active, "darwin") } };

    EXPECT_EQ(countInstallable(index, linuxHost()), 2U);
}
