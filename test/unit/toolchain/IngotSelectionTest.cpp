#include "toolchain/IngotSelection.h"

#include "toolchain/HostEnvironment.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <algorithm>
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
             .cCompiler = "gcc",
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
    EXPECT_EQ(selected->cCompiler, "gcc");
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

TEST(IngotSelectionTest, TellsWhetherAToolchainHasAnIngotForTheHost)
{
    IndexedToolchain elsewhere = toolchain("gcc", "15.2.0");
    elsewhere.ingots.front().arch = "aarch64";

    EXPECT_TRUE(hasIngotFor(toolchain("gcc", "15.2.0", ToolchainStatus::Disabled), linuxHost().environment));
    EXPECT_FALSE(hasIngotFor(elsewhere, linuxHost().environment));
}

/**
 * A toolchain can be installed as install would take it, and for no reason
 * else.
 */
TEST(IngotSelectionTest, TellsWhetherAToolchainCanBeInstalled)
{
    IndexedToolchain needsNewerGlibc = toolchain("gcc", "15.2.0");
    needsNewerGlibc.ingots.front().glibcVersion = version("2.35");
    IndexedToolchain needsNewerScrap = toolchain("gcc", "15.2.0");
    needsNewerScrap.minScrapVersion = version("2.0.0");
    IndexedToolchain elsewhere = toolchain("gcc", "15.2.0");
    elsewhere.ingots.front().platform = "darwin";

    EXPECT_TRUE(canBeInstalled(toolchain("gcc", "15.2.0"), linuxHost()));
    EXPECT_TRUE(canBeInstalled(toolchain("gcc", "15.2.0", ToolchainStatus::Deprecated), linuxHost()));
    EXPECT_FALSE(canBeInstalled(toolchain("gcc", "15.2.0", ToolchainStatus::Disabled), linuxHost()));
    EXPECT_FALSE(canBeInstalled(needsNewerGlibc, linuxHost()));
    EXPECT_FALSE(canBeInstalled(needsNewerScrap, linuxHost()));
    EXPECT_FALSE(canBeInstalled(elsewhere, linuxHost()));
}

TEST(IngotSelectionTest, LooksUpTheToolchainItWouldInstall)
{
    const auto found = lookUpInIndex(gccIndex(), specifier("gcc@15"), linuxHost());

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->toolchain.version.text(), "15.2.0");
    EXPECT_TRUE(found->ingot.has_value());
    EXPECT_FALSE(found->barrier.has_value());
}

TEST(IngotSelectionTest, LooksUpADisabledVersionWithWhatStopsIt)
{
    const auto found = lookUpInIndex(gccIndex(), specifier("gcc@15.3.0"), linuxHost());

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->toolchain.status, ToolchainStatus::Disabled);
    EXPECT_TRUE(found->ingot.has_value());
    ASSERT_TRUE(found->barrier.has_value());
    EXPECT_EQ(std::get<NoLongerOffered>(*found->barrier).alternative, "150.1.0");
}

/**
 * Nothing stops a version the host can install, and a version it cannot is
 * stopped as install would say.
 */
TEST(IngotSelectionTest, TellsWhatStopsOneVersionBeingInstalled)
{
    const ToolchainIndex index = gccIndex();
    const auto versionOf = [&index](const std::string_view version) -> const IndexedToolchain& {
        return *std::ranges::find_if(index.toolchains, [version](const IndexedToolchain& each) {
            return each.version.text() == version;
        });
    };

    EXPECT_FALSE(barrierTo(versionOf("15.2.0"), index, linuxHost()).has_value());
    const auto disabled = barrierTo(versionOf("15.3.0"), index, linuxHost());
    ASSERT_TRUE(disabled.has_value());
    EXPECT_TRUE(std::holds_alternative<NoLongerOffered>(*disabled));
}

TEST(IngotSelectionTest, ReadsNoStatusOfAToolchainNeedingANewerScrap)
{
    EXPECT_TRUE(isStatusRead(std::nullopt));
    EXPECT_TRUE(isStatusRead(NoLongerOffered{ .family = "gcc", .alternative = std::nullopt }));
    EXPECT_FALSE(isStatusRead(NeedsNewerScrap{ .required = "2.0.0" }));
}

TEST(IngotSelectionTest, LooksUpTheNewestVersionWhenNoneCanBeInstalled)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.1.0"), toolchain("gcc", "15.2.0") } };
    index.toolchains[0].ingots.clear();
    index.toolchains[1].ingots.clear();

    const auto found = lookUpInIndex(index, specifier("gcc"), linuxHost());

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->toolchain.version.text(), "15.2.0");
    EXPECT_FALSE(found->ingot.has_value());
    ASSERT_TRUE(found->barrier.has_value());
    EXPECT_EQ(std::get<NoIngotForHost>(*found->barrier).environment, "linux-x86_64");
}

TEST(IngotSelectionTest, FindsNothingToLookUpForAVersionNotInTheIndex)
{
    const auto found = lookUpInIndex(gccIndex(), specifier("gcc@1.0.0"), linuxHost());

    ASSERT_FALSE(found.has_value());
    EXPECT_EQ(found.error().families, (std::vector<std::string>{ "gcc", "llvm" }));
}

TEST(IngotSelectionTest, SuggestsTheNewestInstallableVersionOfAFamily)
{
    const ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.1.0"), toolchain("gcc", "15.2.0", ToolchainStatus::Deprecated), toolchain("llvm", "20.1.0") } };

    EXPECT_EQ(newestInstallable(index, "gcc", linuxHost(), SuggestedStatus::ActiveOrDeprecated)->version.text(), "15.2.0");
    EXPECT_EQ(newestInstallable(index, "gcc", linuxHost(), SuggestedStatus::ActiveOnly)->version.text(), "15.1.0");
}

TEST(IngotSelectionTest, SuggestsNothingWhenNoVersionOfAFamilyCanBeInstalled)
{
    ToolchainIndex index{ .toolchains = { toolchain("gcc", "15.1.0", ToolchainStatus::Disabled), toolchain("gcc", "15.2.0", ToolchainStatus::Deprecated) } };

    EXPECT_EQ(newestInstallable(index, "gcc", linuxHost(), SuggestedStatus::ActiveOnly), nullptr);
    EXPECT_EQ(newestInstallable(index, "llvm", linuxHost(), SuggestedStatus::ActiveOrDeprecated), nullptr);
}

TEST(IngotSelectionTest, TellsWhereAnIngotIsFetchedFrom)
{
    Ingot ingot = hostIngot();
    EXPECT_EQ(sourceOf(ingot), "https://example.com/a");

    ingot.source = RegistrySource{ .registry = "ghcr.io/example", .tag = "gcc:15.2.0", .digest = "sha256:def" };
    EXPECT_EQ(sourceOf(ingot), "ghcr.io/example/gcc:15.2.0");
}
