#include "toolchain/IngotSelection.h"

#include "toolchain/HostEnvironment.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace scrap::Toolchain;

namespace {

ToolchainVersion version(std::string_view text)
{
    return ToolchainVersion::parse(text).value();
}

ToolchainSpecifier specifier(std::string_view text)
{
    return parseToolchainSpecifier(text).value();
}

InstallingHost linuxHost()
{
    return { .environment = { .platform = "linux", .arch = "x86_64" }, .scrapVersion = version("1.0.0"), .glibcVersion = version("2.31") };
}

Ingot hostIngot()
{
    return { .platform = "linux", .arch = "x86_64", .source = UrlSource{ .url = "https://example.com/a", .sha256 = "abc" }, .glibcVersion = std::nullopt };
}

IndexedToolchain toolchain(std::string_view family, std::string_view text, const ToolchainStatus status = ToolchainStatus::Active)
{
    return { .family = std::string{ family },
             .version = version(text),
             .status = status,
             .compiler = "g++",
             .minScrapVersion = std::nullopt,
             .ingots = { hostIngot() } };
}

/** The index of the scenario that shows which version a shortened specifier gets. */
ToolchainIndex gccIndex()
{
    return { .toolchains = { toolchain("gcc", "15.1.0"),
                             toolchain("gcc", "15.3.0", ToolchainStatus::Disabled),
                             toolchain("gcc", "150.1.0"),
                             toolchain("gcc", "15.2.0"),
                             toolchain("llvm", "20.1.0") } };
}

}  // anonymous namespace

TEST(IngotSelectionTest, TakesTheNewestOfferedVersionASpecifierNames)
{
    const ToolchainIndex index = gccIndex();

    EXPECT_EQ(selectIngot(index, specifier("gcc@15"), linuxHost())->version.text(), "15.2.0");
    EXPECT_EQ(selectIngot(index, specifier("gcc@15.1"), linuxHost())->version.text(), "15.1.0");
    EXPECT_EQ(selectIngot(index, specifier("gcc"), linuxHost())->version.text(), "150.1.0");
}

TEST(IngotSelectionTest, GivesWhatIsNeededToInstallTheToolchain)
{
    const auto selected = selectIngot(gccIndex(), specifier("llvm"), linuxHost());

    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(selected->family, "llvm");
    EXPECT_EQ(selected->compiler, "g++");
    EXPECT_EQ(std::get<UrlSource>(selected->ingot.source).url, "https://example.com/a");
}

TEST(IngotSelectionTest, TakesADeprecatedVersion)
{
    const ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.2.0", ToolchainStatus::Deprecated) } };

    EXPECT_TRUE(selectIngot(index, specifier("gcc"), linuxHost()).has_value());
}

TEST(IngotSelectionTest, ListsTheOfferedVersionsWhenNoneMatches)
{
    const auto selected = selectIngot(gccIndex(), specifier("gcc@99"), linuxHost());

    ASSERT_FALSE(selected.has_value());
    const auto& fault = std::get<NoMatchInIndex>(selected.error());
    EXPECT_EQ(fault.versions, (std::vector<std::string>{ "150.1.0", "15.2.0", "15.1.0" }));
}

TEST(IngotSelectionTest, ListsTheFamiliesWhenTheFamilyIsUnknown)
{
    const ToolchainIndex index{ .toolchains = { toolchain("llvm", "20.1.0"), toolchain("gcc", "15.2.0"), toolchain("gcc", "15.1.0") } };

    const auto selected = selectIngot(index, specifier("clang"), linuxHost());

    ASSERT_FALSE(selected.has_value());
    const auto& fault = std::get<NoMatchInIndex>(selected.error());
    EXPECT_TRUE(fault.versions.empty());
    EXPECT_EQ(fault.families, (std::vector<std::string>{ "gcc", "llvm" }));
}

TEST(IngotSelectionTest, RefusesADisabledVersionAndNamesTheNewestToInstall)
{
    const auto selected = selectIngot(gccIndex(), specifier("gcc@15.3.0"), linuxHost());

    ASSERT_FALSE(selected.has_value());
    const auto& fault = std::get<NoLongerOffered>(selected.error());
    EXPECT_EQ(fault.family, "gcc");
    EXPECT_EQ(fault.alternative, "150.1.0");
}

TEST(IngotSelectionTest, HasNoAlternativeWhenEveryVersionIsDisabled)
{
    const ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.3.0", ToolchainStatus::Disabled) } };

    const auto selected = selectIngot(index, specifier("gcc"), linuxHost());

    ASSERT_FALSE(selected.has_value());
    EXPECT_FALSE(std::get<NoLongerOffered>(selected.error()).alternative.has_value());
}

TEST(IngotSelectionTest, PassesOverAVersionWithoutAnIngotForTheHost)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.1.0"), toolchain("gcc", "15.2.0") } };
    index.toolchains[1].ingots[0].arch = "aarch64";

    EXPECT_EQ(selectIngot(index, specifier("gcc"), linuxHost())->version.text(), "15.1.0");
}

TEST(IngotSelectionTest, MatchesTheIngotToBothPlatformAndArchitecture)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.1.0") } };
    index.toolchains[0].ingots[0].platform = "darwin";

    const auto selected = selectIngot(index, specifier("gcc"), linuxHost());

    ASSERT_FALSE(selected.has_value());
    EXPECT_TRUE(std::holds_alternative<NoIngotForHost>(selected.error()));
}

TEST(IngotSelectionTest, ListsTheVersionsThatHaveAnIngotForTheHost)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.1.0"), toolchain("gcc", "15.2.0"), toolchain("gcc", "15.3.0", ToolchainStatus::Disabled) } };
    index.toolchains[1].ingots.clear();

    const auto selected = selectIngot(index, specifier("gcc@15.2.0"), linuxHost());

    ASSERT_FALSE(selected.has_value());
    EXPECT_EQ(std::get<NoIngotForHost>(selected.error()).versions, (std::vector<std::string>{ "15.1.0" }));
}

TEST(IngotSelectionTest, RefusesAnIngotThatNeedsANewerGlibc)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.2.0") } };
    index.toolchains[0].ingots[0].glibcVersion = version("2.40");

    const auto selected = selectIngot(index, specifier("gcc@15.2.0"), linuxHost());

    ASSERT_FALSE(selected.has_value());
    const auto& fault = std::get<NeedsNewerGlibc>(selected.error());
    EXPECT_EQ(fault.required, "2.40");
    EXPECT_EQ(fault.host, "2.31");
}

TEST(IngotSelectionTest, TakesAnIngotThatNeedsTheGlibcTheHostHas)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.2.0") } };
    index.toolchains[0].ingots[0].glibcVersion = version("2.31");

    EXPECT_TRUE(selectIngot(index, specifier("gcc"), linuxHost()).has_value());
}

TEST(IngotSelectionTest, PassesOverAVersionThatNeedsANewerGlibc)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.1.0"), toolchain("gcc", "15.2.0") } };
    index.toolchains[1].ingots[0].glibcVersion = version("2.40");

    EXPECT_EQ(selectIngot(index, specifier("gcc@15"), linuxHost())->version.text(), "15.1.0");
}

TEST(IngotSelectionTest, ComparesNoGlibcOnAHostWithoutOne)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.2.0") } };
    index.toolchains[0].ingots[0].glibcVersion = version("2.40");
    InstallingHost host = linuxHost();
    host.glibcVersion = std::nullopt;

    EXPECT_TRUE(selectIngot(index, specifier("gcc"), host).has_value());
}

TEST(IngotSelectionTest, RefusesAToolchainThatNeedsANewerScrap)
{
    // The index parser keeps such a toolchain disabled with no ingots.
    IndexedToolchain newer = toolchain("gcc", "15.2.0", ToolchainStatus::Disabled);
    newer.minScrapVersion = version("2.0.0");
    newer.ingots.clear();
    const ToolchainIndex index{ .toolchains = { newer } };

    const auto selected = selectIngot(index, specifier("gcc@15.2.0"), linuxHost());

    ASSERT_FALSE(selected.has_value());
    EXPECT_EQ(std::get<NeedsNewerScrap>(selected.error()).required, "2.0.0");
}

TEST(IngotSelectionTest, TakesAToolchainThatNeedsTheScrapRunning)
{
    IndexedToolchain same = toolchain("gcc", "15.2.0");
    same.minScrapVersion = version("1.0.0");
    const ToolchainIndex index{ .toolchains = { same } };

    EXPECT_TRUE(selectIngot(index, specifier("gcc"), linuxHost()).has_value());
}

TEST(IngotSelectionTest, ReportsWhatStopsTheNewestWhenNoneCanBeInstalled)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.1.0"), toolchain("gcc", "15.2.0", ToolchainStatus::Disabled) } };
    index.toolchains[0].ingots[0].glibcVersion = version("2.40");

    const auto selected = selectIngot(index, specifier("gcc"), linuxHost());

    ASSERT_FALSE(selected.has_value());
    const auto& fault = std::get<NoLongerOffered>(selected.error());
    EXPECT_FALSE(fault.alternative.has_value());
}

TEST(IngotSelectionTest, GivesTheValueThatChecksAnIngot)
{
    Ingot ingot = hostIngot();
    EXPECT_EQ(integrityOf(ingot), "abc");

    ingot.source = RegistrySource{ .registry = "ghcr.io/example", .tag = "gcc:15.2.0", .digest = "sha256:def" };
    EXPECT_EQ(integrityOf(ingot), "sha256:def");
}
