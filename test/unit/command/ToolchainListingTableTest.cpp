#include "command/ToolchainListingTable.h"

#include "command/ToolchainListing.h"
#include "command/ToolchainNaming.h"
#include "toolchain/SystemCompiler.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>

using namespace scrap::Command;
using scrap::Toolchain::CompilerOrigin;

namespace {

ListedToolchain listed(const std::string& name)
{
    return ListedToolchain{ .name = name, .isSystem = false, .compiler = std::nullopt, .present = true, .isDefault = false, .usedHere = std::nullopt };
}

ListedToolchain systemAt(const std::filesystem::path& compiler)
{
    ListedToolchain toolchain = listed("system");
    toolchain.isSystem = true;
    toolchain.compiler = compiler;
    return toolchain;
}

}  // namespace

/**
 * In a project, the default and the one used here are in columns of their
 * own, each as wide as its widest cell, with no space at a line's end.
 */
TEST(ToolchainListingTableTest, ShowsTheDefaultAndTheOneUsedHereInColumns)
{
    ListedToolchain used = listed("gcc@15.2.0");
    used.usedHere = ChoiceSource::Project;
    ListedToolchain chosen = listed("gcc@16.2.0");
    chosen.isDefault = true;

    EXPECT_EQ(renderToolchainListingTable({ .toolchains = { systemAt("/usr/bin/c++"), used, chosen }, .inProject = true }),
              "TOOLCHAIN              DEFAULT  USED HERE\n"
              "system (/usr/bin/c++)\n"
              "gcc@15.2.0                      from scrap.toml\n"
              "gcc@16.2.0             yes\n");
}

/**
 * Outside a project there is no USED HERE column.
 */
TEST(ToolchainListingTableTest, LeavesOutTheUsedHereColumnOutsideAProject)
{
    ListedToolchain chosen = listed("gcc@16.2.0");
    chosen.isDefault = true;

    EXPECT_EQ(renderToolchainListingTable({ .toolchains = { listed("gcc@15.2.0"), chosen }, .inProject = false }),
              "TOOLCHAIN   DEFAULT\n"
              "gcc@15.2.0\n"
              "gcc@16.2.0  yes\n");
}

/**
 * The system's compiler used where nothing names a toolchain says how it
 * was found, as a build says it.
 */
TEST(ToolchainListingTableTest, SaysHowTheSystemCompilerUsedHereWasFound)
{
    ListedToolchain found = systemAt("/usr/bin/c++");
    found.usedHere = CompilerOrigin::DefaultOnPath;

    EXPECT_EQ(renderToolchainListingTable({ .toolchains = { found }, .inProject = true }),
              "TOOLCHAIN              DEFAULT  USED HERE\n"
              "system (/usr/bin/c++)           found on PATH\n");
}

/**
 * A toolchain named that is not there says so after each mark: not
 * installed, or for system not found.
 */
TEST(ToolchainListingTableTest, SaysWhatANamedToolchainLacks)
{
    ListedToolchain missing = listed("gcc@15.2.0");
    missing.present = false;
    missing.isDefault = true;
    missing.usedHere = ChoiceSource::VariableOverProject;
    ListedToolchain noSystem = listed("system");
    noSystem.isSystem = true;
    noSystem.present = false;
    noSystem.usedHere = ChoiceSource::GlobalDefault;

    EXPECT_EQ(renderToolchainListingTable({ .toolchains = { missing, noSystem }, .inProject = true }),
              "TOOLCHAIN   DEFAULT             USED HERE\n"
              "gcc@15.2.0  yes, not installed  from SCRAP_TOOLCHAIN, overriding scrap.toml, not installed\n"
              "system                          from the global default, not found\n");
}

/**
 * A name as written is shown with what cannot be printed escaped.
 */
TEST(ToolchainListingTableTest, EscapesANameAsWritten)
{
    ListedToolchain written = listed("gcc@15\x1b");
    written.present = false;
    written.isDefault = true;

    EXPECT_EQ(renderToolchainListingTable({ .toolchains = { written }, .inProject = false }),
              "TOOLCHAIN   DEFAULT\n"
              "gcc@15\\x1B  yes, not installed\n");
}
