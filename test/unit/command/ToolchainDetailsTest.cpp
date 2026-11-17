#include "command/ToolchainDetails.h"

#include "command/ToolchainListing.h"
#include "command/ToolchainNaming.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

using namespace scrap::Command;

namespace {

ListedToolchain listed(const std::string& name)
{
    return ListedToolchain{ .name = name, .isSystem = false, .compiler = std::nullopt, .present = true, .isDefault = false, .usedHere = std::nullopt };
}

}  // anonymous namespace

TEST(ToolchainDetailsTest, MarksAToolchainAsTheListingMarksIt)
{
    ListedToolchain used = listed("gcc@15.2.0");
    used.isDefault = true;
    used.usedHere = ChoiceSource::Project;
    const ToolchainListing listing{ .toolchains = { listed("gcc@14.1.0"), used }, .inProject = true };

    const ToolchainSelection selection = selectionIn(listing, "gcc@15.2.0");

    EXPECT_TRUE(selection.isDefault);
    EXPECT_EQ(selection.usedHere, std::optional<UseReason>{ ChoiceSource::Project });
    EXPECT_TRUE(selection.inProject);
    EXPECT_FALSE(selectionIn(listing, "gcc@14.1.0").isDefault);
}

TEST(ToolchainDetailsTest, CarriesWhetherWhatNamesTheToolchainHereCanBeRead)
{
    const ToolchainListing listing{ .toolchains = { listed("gcc@15.2.0") }, .inProject = true, .hereUnreadable = true };

    EXPECT_TRUE(selectionIn(listing, "gcc@15.2.0").hereUnreadable);
}

TEST(ToolchainDetailsTest, PassesOverANameTheListingHasAsNotThere)
{
    ListedToolchain missing = listed("gcc@15.2.0");
    missing.present = false;
    missing.isDefault = true;
    const ToolchainListing listing{ .toolchains = { missing }, .inProject = false };

    const ToolchainSelection selection = selectionIn(listing, "gcc@15.2.0");

    EXPECT_FALSE(selection.isDefault);
    EXPECT_FALSE(selection.usedHere.has_value());
    EXPECT_FALSE(selection.inProject);
}

TEST(ToolchainDetailsTest, MarksAToolchainNotInstalledAsTheNamesLeadingToItAreMarked)
{
    ListedToolchain byDefault = listed("gcc");
    byDefault.present = false;
    byDefault.isDefault = true;
    ListedToolchain byProject = listed("gcc@15");
    byProject.present = false;
    byProject.usedHere = ChoiceSource::Project;
    const ToolchainListing listing{ .toolchains = { listed("gcc@15.2.0"), byDefault, byProject }, .inProject = true, .hereUnreadable = false };

    const ToolchainSelection selection = selectionOfNotInstalled(listing, [](std::string_view) {
        return true;
    });

    EXPECT_TRUE(selection.isDefault);
    EXPECT_EQ(selection.usedHere, std::optional<UseReason>{ ChoiceSource::Project });
    EXPECT_TRUE(selection.inProject);
}

TEST(ToolchainDetailsTest, PassesOverNamesThatLeadElsewhereAndInstalledToolchains)
{
    ListedToolchain elsewhere = listed("gcc@14");
    elsewhere.present = false;
    elsewhere.isDefault = true;
    ListedToolchain installedHere = listed("gcc@15.2.0");
    installedHere.usedHere = ChoiceSource::Project;
    const ToolchainListing listing{ .toolchains = { elsewhere, installedHere }, .inProject = true, .hereUnreadable = false };

    const ToolchainSelection selection = selectionOfNotInstalled(listing, [](std::string_view name) {
        return name != "gcc@14";
    });

    EXPECT_FALSE(selection.isDefault);
    EXPECT_FALSE(selection.usedHere.has_value());
}
