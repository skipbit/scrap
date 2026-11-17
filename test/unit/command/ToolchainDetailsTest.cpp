#include "command/ToolchainDetails.h"

#include "command/ToolchainListing.h"
#include "command/ToolchainNaming.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

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
