#include "command/ToolchainDiagnostic.h"

#include "toolchain/GlobalConfigFile.h"
#include "toolchain/InstalledToolchain.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <system_error>

using namespace scrap::Command;
using namespace scrap::Toolchain;

namespace {

const std::filesystem::path File = "/home/me/scrap/config.toml";
const std::string PermissionLine = "hint: check the permissions of the path\n";

}  // namespace

/**
 * Without a place to keep toolchains, the text names both variables and the
 * one to set.
 */
TEST(ToolchainDiagnosticTest, RendersNoScrapHome)
{
    EXPECT_EQ(renderNoScrapHome(),
              "error: cannot tell where to keep toolchains: SCRAP_HOME and HOME are both unset\n"
              "hint: set SCRAP_HOME to the directory to keep them in, and run the command again\n");
}

/**
 * A fault in the text is pointed at as a compiler points at one, with the
 * key when there is one.
 */
TEST(ToolchainDiagnosticTest, RendersAFaultInTheConfigText)
{
    EXPECT_EQ(renderGlobalConfigFileError(File, GlobalConfigFault{ .line = 2, .column = 13, .key = "defaults.toolchain", .message = "expected a string" }),
              "/home/me/scrap/config.toml:2:13: error: defaults.toolchain: expected a string\n"
              "hint: fix the file, or remove it to clear the settings it holds\n");
    EXPECT_EQ(renderGlobalConfigFileError(File, GlobalConfigFault{ .line = 1, .column = 10, .key = "", .message = "unexpected end of line" }),
              "/home/me/scrap/config.toml:1:10: error: unexpected end of line\n"
              "hint: fix the file, or remove it to clear the settings it holds\n");
}

/**
 * A file that cannot be read names its path and points at permissions.
 */
TEST(ToolchainDiagnosticTest, RendersAConfigFileThatCannotBeRead)
{
    EXPECT_EQ(renderGlobalConfigFileError(File, CannotReadConfig{ .path = File }), "error: cannot read '/home/me/scrap/config.toml'\n" + PermissionLine);
}

/**
 * A directory or file that cannot be written names its path and the
 * operating system's reason.
 */
TEST(ToolchainDiagnosticTest, RendersAConfigFileThatCannotBeWritten)
{
    const std::error_code denied = std::make_error_code(std::errc::permission_denied);

    EXPECT_EQ(renderGlobalConfigFileError(File, CannotCreateDirectory{ .path = File.parent_path(), .code = denied }),
              "error: cannot create '/home/me/scrap': " + denied.message() + "\n" + PermissionLine);
    EXPECT_EQ(renderGlobalConfigFileError(File, CannotWriteConfig{ .path = File, .code = denied }),
              "error: cannot write '/home/me/scrap/config.toml': " + denied.message() + "\n" + PermissionLine);
}

/**
 * Each reason no installed toolchain can be used names the specifier as it
 * was written, where it needs one, and the command that fixes it.
 */
TEST(ToolchainDiagnosticTest, RendersWhyNoInstalledToolchainCanBeUsed)
{
    EXPECT_EQ(renderInstalledToolchainFault("gcc@15", ToolchainNamedIn::Variable, NoToolchainForHost{}), renderNoToolchainForHost(ToolchainNamedIn::Variable));
    EXPECT_EQ(renderInstalledToolchainFault("gcc@15", ToolchainNamedIn::CommandLine, UnusableInstalled{ .reason = "its record is broken" }),
              "error: cannot use the installed toolchain: its record is broken\n"
              "hint: run 'scrap toolchain install --force gcc@15'\n");
    EXPECT_EQ(renderInstalledToolchainFault("gcc@15", ToolchainNamedIn::CommandLine, NotInstalled{}),
              "error: toolchain 'gcc@15' is not installed\n"
              "hint: run 'scrap toolchain install gcc@15'\n");
    const std::error_code denied = std::make_error_code(std::errc::permission_denied);
    EXPECT_EQ(renderInstalledToolchainFault("gcc@15", ToolchainNamedIn::CommandLine, CannotListToolchains{ .directory = "/home/me/.scrap/toolchains/gcc", .code = denied }),
              "error: cannot list '/home/me/.scrap/toolchains/gcc': " + denied.message() + "\n" + PermissionLine);
}

/**
 * A system with no compiler names both ways to give it one: installing a
 * compiler, or pointing the environment at the one to use.
 */
TEST(ToolchainDiagnosticTest, RendersASystemWithNoCompiler)
{
    EXPECT_EQ(renderNoCompilerFound(),
              "error: no C++ compiler found\n"
              "hint: install a C++ compiler, or set CXX to the one to use\n");
}

/**
 * A compiler the environment asked for and cannot be run names the value it
 * gave, and both ways out: pointing CXX at a program, or leaving it unset.
 */
TEST(ToolchainDiagnosticTest, RendersACompilerRequestThatCannotBeRun)
{
    EXPECT_EQ(renderUnusableCompilerRequest("ccache g++"),
              "error: CXX names 'ccache g++', which cannot be run\n"
              "hint: set CXX to the path of a compiler, or unset it to search for one\n");
}

/**
 * The value reaches the terminal as text, since the environment can hold any
 * byte and an escape sequence would otherwise be acted on.
 */
TEST(ToolchainDiagnosticTest, EscapesAControlCharacterInACompilerRequest)
{
    EXPECT_EQ(renderUnusableCompilerRequest("g++\x1b[31m"),
              "error: CXX names 'g++\\x1B[31m', which cannot be run\n"
              "hint: set CXX to the path of a compiler, or unset it to search for one\n");
}

/**
 * A host without toolchains is answered with how to choose the system's
 * compiler where the toolchain was named, since a place earlier in the
 * order would otherwise still name it.
 */
TEST(ToolchainDiagnosticTest, RendersAHostWithoutToolchainsByWhereTheToolchainWasNamed)
{
    const std::string error = "error: no toolchain is built for this host\n";
    const std::string selectSystem = "hint: run 'scrap toolchain select system' to use the compiler found on the system\n";
    EXPECT_EQ(renderNoToolchainForHost(ToolchainNamedIn::CommandLine), error + selectSystem);
    EXPECT_EQ(renderNoToolchainForHost(ToolchainNamedIn::GlobalDefault), error + selectSystem);
    EXPECT_EQ(renderNoToolchainForHost(ToolchainNamedIn::Variable),
              error + "hint: set SCRAP_TOOLCHAIN to system, or unset it, to use the compiler found on the system\n");
    EXPECT_EQ(renderNoToolchainForHost(ToolchainNamedIn::Project),
              error + "hint: set name under [toolchain] in scrap.toml to \"system\" to use the compiler found on the system\n");
}
