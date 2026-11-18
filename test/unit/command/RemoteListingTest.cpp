#include "command/RemoteListing.h"

#include "toolchain/HostEnvironment.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace scrap::Command;
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

IndexedToolchain toolchain(std::string_view family, std::string_view text, const ToolchainStatus status = ToolchainStatus::Active)
{
    return { .family = std::string{ family },
             .version = version(text),
             .status = status,
             .compiler = "g++",
             .cCompiler = "gcc",
             .minScrapVersion = std::nullopt,
             .ingots = { Ingot{ .platform = "linux",
                                .arch = "x86_64",
                                .source = UrlSource{ .url = "https://example.com/a", .sha256 = "abc" },
                                .glibcVersion = std::nullopt } } };
}

/** An index of two families, given out of order, with one version disabled and one not built for the host. */
ToolchainIndex mixedIndex()
{
    ToolchainIndex index{ .toolchains = { toolchain("llvm", "20.1.0"),
                                          toolchain("gcc", "15.2.0"),
                                          toolchain("gcc", "15.3.0", ToolchainStatus::Disabled),
                                          toolchain("gcc", "15.10.0", ToolchainStatus::Deprecated),
                                          toolchain("llvm", "17.0.1") } };
    index.toolchains[4].ingots.front().arch = "aarch64";
    return index;
}

std::vector<std::string> namesOf(const std::vector<RemoteToolchain>& rows)
{
    std::vector<std::string> names;
    for (const RemoteToolchain& row : rows) {
        names.push_back(row.name);
    }
    return names;
}

}  // anonymous namespace

/**
 * Every version is listed, by family and then from the newest, with what
 * stops the host installing it.
 */
TEST(RemoteListingTest, ListsEveryVersionWithWhatStopsTheHost)
{
    const auto rows = listRemoteToolchains(mixedIndex(), {}, linuxHost());

    EXPECT_EQ(namesOf(rows), (std::vector<std::string>{ "gcc@15.10.0", "gcc@15.3.0", "gcc@15.2.0", "llvm@20.1.0", "llvm@17.0.1" }));
    ASSERT_TRUE(rows[1].barrier.has_value());
    EXPECT_TRUE(std::holds_alternative<NoLongerOffered>(*rows[1].barrier));
    ASSERT_TRUE(rows[4].barrier.has_value());
    EXPECT_TRUE(std::holds_alternative<NoIngotForHost>(*rows[4].barrier));
    EXPECT_FALSE(rows[2].barrier.has_value());
    EXPECT_EQ(rows.front().status, ToolchainStatus::Deprecated);
}

TEST(RemoteListingTest, KeepsOnlyWhatTheHostCanInstall)
{
    const auto rows = installableOnly(listRemoteToolchains(mixedIndex(), {}, linuxHost()));

    EXPECT_EQ(namesOf(rows), (std::vector<std::string>{ "gcc@15.10.0", "gcc@15.2.0", "llvm@20.1.0" }));
    EXPECT_EQ(rows.front().status, ToolchainStatus::Deprecated);
    EXPECT_FALSE(rows.front().barrier.has_value());
}

TEST(RemoteListingTest, MarksWhatIsInstalled)
{
    const std::vector<InstalledName> installed{ { .family = "gcc", .version = version("15.2.0") }, { .family = "llvm", .version = version("17.0.1") } };

    const auto rows = installableOnly(listRemoteToolchains(mixedIndex(), installed, linuxHost()));

    ASSERT_EQ(rows.size(), 3);
    EXPECT_FALSE(rows[0].installed);
    EXPECT_TRUE(rows[1].installed);
    EXPECT_FALSE(rows[2].installed);
}

TEST(RemoteListingTest, NamesAnIndexedToolchainByFamilyAndVersion)
{
    EXPECT_EQ(toolchain("gcc", "15.2.0").text(), "gcc@15.2.0");
}

/**
 * A version needing a newer scrap was not read as far as its status, so it
 * is listed with none.
 */
TEST(RemoteListingTest, ListsNoStatusForAVersionNeedingANewerScrap)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "16.1.0", ToolchainStatus::Disabled) } };
    index.toolchains.front().minScrapVersion = version("2.0.0");

    const auto rows = listRemoteToolchains(index, {}, linuxHost());

    ASSERT_EQ(rows.size(), 1);
    EXPECT_FALSE(rows.front().status.has_value());
    ASSERT_TRUE(rows.front().barrier.has_value());
    EXPECT_TRUE(std::holds_alternative<NeedsNewerScrap>(*rows.front().barrier));
}
