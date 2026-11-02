#include "toolchain/InstalledToolchain.h"

#include "support/TempDirectory.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace scrap::Toolchain;
using scrap::TestSupport::TempDirectory;

namespace {

const HostEnvironment Host{ .platform = "linux", .arch = "x86_64" };

/**
 * Lay out @p family @p version for @p hostDirectory below @p home as an
 * install would, with its compiler at bin/g++.
 */
void install(const TempDirectory& home, std::string_view family, std::string_view version, std::string_view hostDirectory = "linux-x86_64")
{
    const std::string directory = "toolchains/" + std::string{ family } + "/" + std::string{ version } + "/" + std::string{ hostDirectory };
    home.writeFile(directory + "/bin/g++", "");
    home.writeFile(directory + "/" + std::string{ InstallRecordName }, "compiler = \"bin/g++\"\n");
}

/**
 * The version of the toolchain @p specifier finds below @p home, or
 * "<none>" when it finds none.
 */
std::string versionFound(const TempDirectory& home, std::string_view specifier)
{
    const auto found = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier(specifier).value());
    EXPECT_TRUE(found.has_value()) << found.error();
    if (! found.has_value() || ! found->has_value()) {
        return "<none>";
    }
    return (*found)->version.text();
}

}  // namespace

/**
 * Without a toolchains directory, or without the family, nothing is found.
 */
TEST(InstalledToolchainTest, FindsNothingWhenNoneIsInstalled)
{
    const TempDirectory home;
    EXPECT_EQ(versionFound(home, "gcc"), "<none>");

    install(home, "llvm", "22.1.0");
    EXPECT_EQ(versionFound(home, "gcc"), "<none>");
}

/**
 * Of the installed versions the specifier names, the newest is found, with
 * its directory and its compiler.
 */
TEST(InstalledToolchainTest, FindsTheNewestTheSpecifierNames)
{
    const TempDirectory home;
    install(home, "gcc", "15.1.0");
    install(home, "gcc", "15.2.0");
    install(home, "gcc", "150.1.0");

    EXPECT_EQ(versionFound(home, "gcc@15"), "15.2.0");
    EXPECT_EQ(versionFound(home, "gcc@15.1"), "15.1.0");
    EXPECT_EQ(versionFound(home, "gcc"), "150.1.0");
    EXPECT_EQ(versionFound(home, "gcc@14"), "<none>");

    const auto found = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("gcc@15.2.0").value());
    ASSERT_TRUE(found.has_value() && found->has_value());
    EXPECT_EQ((*found)->family, "gcc");
    EXPECT_EQ((*found)->directory, home.path() / "toolchains/gcc/15.2.0/linux-x86_64");
    EXPECT_EQ((*found)->compiler, home.path() / "toolchains/gcc/15.2.0/linux-x86_64/bin/g++");
}

/**
 * A version without an install record, one for another host, and a
 * directory whose name is not a version are passed over.
 */
TEST(InstalledToolchainTest, PassesOverWhatIsNotInstalledForTheHost)
{
    const TempDirectory home;
    install(home, "gcc", "15.1.0");
    home.makeDirectory("toolchains/gcc/15.2.0/linux-x86_64/bin");
    install(home, "gcc", "15.3.0", "darwin-aarch64");
    install(home, "gcc", "staging");

    EXPECT_EQ(versionFound(home, "gcc"), "15.1.0");
}

/**
 * When the newest one's record cannot be used, that is reported rather than
 * an older one chosen in its place.
 */
TEST(InstalledToolchainTest, RefusesTheNewestWhenItsRecordCannotBeUsed)
{
    const TempDirectory home;
    install(home, "gcc", "15.1.0");
    install(home, "gcc", "15.2.0");
    home.writeFile("toolchains/gcc/15.2.0/linux-x86_64/" + std::string{ InstallRecordName }, "compiler = 1\n");

    const auto broken = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("gcc").value());
    ASSERT_FALSE(broken.has_value());
    EXPECT_TRUE(broken.error().ends_with("line 1: compiler: must be a string")) << broken.error();

    home.writeFile("toolchains/gcc/15.2.0/linux-x86_64/" + std::string{ InstallRecordName }, "compiler = \"bin/clang++\"\n");
    const auto missing = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("gcc").value());
    ASSERT_FALSE(missing.has_value());
    EXPECT_TRUE(missing.error().ends_with("bin/clang++', which is not there")) << missing.error();
}

/**
 * The runtime library directories the record names are found inside the
 * toolchain; one that is not there makes the toolchain unusable.
 */
TEST(InstalledToolchainTest, FindsTheRuntimeLibraryDirectories)
{
    const TempDirectory home;
    install(home, "gcc", "15.2.0");
    const std::string directory = "toolchains/gcc/15.2.0/linux-x86_64";
    home.writeFile(directory + "/lib64/libstdc++.so.6", "");
    home.writeFile(directory + "/" + std::string{ InstallRecordName }, "compiler = \"bin/g++\"\nruntime-library-dirs = [\"lib64\"]\n");

    const auto found = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("gcc").value());
    ASSERT_TRUE(found.has_value()) << found.error();
    ASSERT_TRUE(found->has_value());
    const std::vector<std::filesystem::path> expected{ home.path() / directory / "lib64" };
    EXPECT_EQ((*found)->runtimeLibraryDirectories, expected);

    home.writeFile(directory + "/" + std::string{ InstallRecordName }, "compiler = \"bin/g++\"\nruntime-library-dirs = [\"lib\"]\n");
    const auto missing = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("gcc").value());
    ASSERT_FALSE(missing.has_value());
    EXPECT_TRUE(missing.error().ends_with("/lib', which is not there")) << missing.error();
}

/**
 * A toolchain is named as the specifier that selects it is written.
 */
TEST(InstalledToolchainTest, NamesItselfAsFamilyAndVersion)
{
    const InstalledToolchain toolchain{
        .family = "gcc", .version = ToolchainVersion::parse("15.2.0").value(), .directory = "", .compiler = "", .runtimeLibraryDirectories = {}
    };

    EXPECT_EQ(toolchain.name(), "gcc@15.2.0");
}

/**
 * The toolchain found for the host carries what was installed.
 */
TEST(InstalledToolchainTest, FindsTheToolchainForTheHost)
{
    const auto host = hostEnvironment();
    if (! host.has_value()) {
        GTEST_SKIP() << "no toolchain is built for this host";
    }
    const TempDirectory home;
    install(home, "gcc", "15.2.0", host->directoryName());

    const auto found = findInstalledToolchainForHost(home.path(), parseToolchainSpecifier("gcc@15").value());

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name(), "gcc@15.2.0");
}

/**
 * A specifier that nothing installed names is reported as not installed.
 */
TEST(InstalledToolchainTest, ReportsAToolchainThatIsNotInstalled)
{
    if (! hostEnvironment().has_value()) {
        GTEST_SKIP() << "no toolchain is built for this host";
    }
    const TempDirectory home;

    const auto found = findInstalledToolchainForHost(home.path(), parseToolchainSpecifier("gcc").value());

    ASSERT_FALSE(found.has_value());
    EXPECT_TRUE(std::holds_alternative<NotInstalled>(found.error()));
}

/**
 * An installed toolchain whose record cannot be used is reported with the
 * reason.
 */
TEST(InstalledToolchainTest, ReportsAnInstalledToolchainThatCannotBeUsed)
{
    const auto host = hostEnvironment();
    if (! host.has_value()) {
        GTEST_SKIP() << "no toolchain is built for this host";
    }
    const TempDirectory home;
    install(home, "gcc", "15.2.0", host->directoryName());
    home.writeFile("toolchains/gcc/15.2.0/" + host->directoryName() + "/" + std::string{ InstallRecordName }, "compiler = \"bin/missing\"\n");

    const auto found = findInstalledToolchainForHost(home.path(), parseToolchainSpecifier("gcc").value());

    ASSERT_FALSE(found.has_value());
    const auto* unusable = std::get_if<UnusableInstalled>(&found.error());
    ASSERT_NE(unusable, nullptr);
    EXPECT_TRUE(unusable->reason.ends_with("which is not there")) << unusable->reason;
}
