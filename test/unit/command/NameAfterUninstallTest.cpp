#include "command/NameAfterUninstall.h"

#include "command/ToolchainNaming.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ToolchainChoice.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace scrap::Command;
using scrap::Toolchain::InstalledName;
using scrap::Toolchain::parseToolchainChoice;
using scrap::Toolchain::ToolchainVersion;

namespace {

InstalledName installed(const std::string& family, std::string_view version)
{
    return InstalledName{ .family = family, .version = ToolchainVersion::parse(version).value() };
}

NamedToolchain named(const std::string& text)
{
    return NamedToolchain{ .text = text, .choice = parseToolchainChoice(text).value(), .source = ChoiceSource::GlobalDefault };
}

/**
 * Where @p text leads once @p removed goes, as a test reads it: "nowhere",
 * the name it leads to, or "unchanged".
 */
std::string leadsAfter(const std::string& text, const std::vector<InstalledName>& before, std::string_view removed)
{
    const auto after = nameAfterUninstall(named(text), before, removed);
    if (! after.has_value()) {
        return "unchanged";
    }
    if (const auto* elsewhere = std::get_if<LeadsElsewhere>(&*after)) {
        return elsewhere->name;
    }
    return "nowhere";
}

}  // anonymous namespace

TEST(NameAfterUninstallTest, LeadsNowhereWhenNothingElseIsNamed)
{
    const std::vector<InstalledName> before{ installed("gcc", "15.2.0"), installed("gcc", "16.2.0") };

    EXPECT_EQ(leadsAfter("gcc@16.2.0", before, "gcc@16.2.0"), "nowhere");
    EXPECT_EQ(leadsAfter("gcc@16", before, "gcc@16.2.0"), "nowhere");
}

TEST(NameAfterUninstallTest, LeadsToTheNewestVersionLeft)
{
    const std::vector<InstalledName> before{ installed("gcc", "15.2.0"), installed("gcc", "15.10.0"), installed("gcc", "16.2.0") };

    EXPECT_EQ(leadsAfter("gcc", before, "gcc@16.2.0"), "gcc@15.10.0");
}

/**
 * A name that led elsewhere before, or to the system's compiler, is left
 * as it is.
 */
TEST(NameAfterUninstallTest, LeavesANameThatDidNotLeadToTheRemovedToolchain)
{
    const std::vector<InstalledName> before{ installed("gcc", "15.2.0"), installed("gcc", "16.2.0") };

    EXPECT_EQ(leadsAfter("gcc", before, "gcc@15.2.0"), "unchanged");
    EXPECT_EQ(leadsAfter("gcc@15", before, "gcc@16.2.0"), "unchanged");
    EXPECT_EQ(leadsAfter("system", before, "gcc@16.2.0"), "unchanged");
}
