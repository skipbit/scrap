#include "command/ToolchainUninstallText.h"

#include "command/NameAfterUninstall.h"
#include "command/ToolchainNaming.h"
#include "install/ToolchainRemoval.h"
#include "toolchain/ToolchainChoice.h"

#include <gtest/gtest.h>

#include <string>
#include <system_error>

using namespace scrap::Command;
using scrap::Toolchain::parseToolchainChoice;

namespace {

NamedToolchain named(const std::string& text, const ChoiceSource source)
{
    return NamedToolchain{ .text = text, .choice = parseToolchainChoice(text).value(), .source = source };
}

}  // anonymous namespace

TEST(ToolchainUninstallTextTest, SaysWhereTheToolchainWasTakenFrom)
{
    EXPECT_EQ(renderUninstalled("gcc@15.2.0", "/home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64"),
              "Uninstalled 'gcc@15.2.0' from '/home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64'\n");
}

TEST(ToolchainUninstallTextTest, SaysWhatBecameOfTheGlobalDefault)
{
    EXPECT_EQ(renderGlobalDefaultAfterUninstall(named("gcc@16.2.0", ChoiceSource::GlobalDefault), LeadsNowhere{}),
              "Unselected 'gcc@16.2.0'; no toolchain is selected globally\n");
    EXPECT_EQ(renderGlobalDefaultAfterUninstall(named("gcc", ChoiceSource::GlobalDefault), LeadsElsewhere{ .name = "gcc@15.2.0" }),
              "The global default 'gcc' now leads to 'gcc@15.2.0'\n");
}

TEST(ToolchainUninstallTextTest, WarnsThatBuildsHereStop)
{
    EXPECT_EQ(renderUsedHereAfterUninstall(named("gcc@16", ChoiceSource::Project), LeadsNowhere{}),
              "warning: scrap.toml names 'gcc@16', which is no longer installed; builds here stop until it is installed again\n"
              "hint: run 'scrap toolchain install gcc@16'\n");
    EXPECT_EQ(renderUsedHereAfterUninstall(named("gcc@16", ChoiceSource::VariableOverProject), LeadsNowhere{}).substr(0, 41),
              "warning: SCRAP_TOOLCHAIN names 'gcc@16', ");
}

TEST(ToolchainUninstallTextTest, SaysWhatBuildsHereUseNow)
{
    EXPECT_EQ(renderUsedHereAfterUninstall(named("gcc", ChoiceSource::Variable), LeadsElsewhere{ .name = "gcc@15.2.0" }),
              "SCRAP_TOOLCHAIN names 'gcc', which now leads to 'gcc@15.2.0'\n");
}

TEST(ToolchainUninstallTextTest, RefusesWhatItCannotUninstall)
{
    EXPECT_EQ(renderCannotUninstallSystem(), "error: 'system' cannot be uninstalled; it is the system's compiler, which scrap does not install\n");
    EXPECT_EQ(renderNothingToUninstall("gcc@15.2.0"),
              "error: toolchain 'gcc@15.2.0' is not installed\n"
              "hint: run 'scrap toolchain list' to see what is installed\n");
    EXPECT_EQ(renderNamesMoreThanOneToUninstall("gcc@15", { "gcc@15.1.0", "gcc@15.2.0" }),
              "error: 'gcc@15' names more than one installed toolchain: gcc@15.1.0, gcc@15.2.0\n"
              "hint: name one with its full version, as in 'scrap toolchain uninstall gcc@15.2.0'\n");
}

TEST(ToolchainUninstallTextTest, SaysWhatCouldNotBeRemovedAndWhy)
{
    const std::error_code denied = std::make_error_code(std::errc::permission_denied);
    EXPECT_EQ(renderNotRemoved({ .path = "/home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64", .code = denied }),
              "error: cannot remove '/home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64': " + denied.message() + "\n");
}
