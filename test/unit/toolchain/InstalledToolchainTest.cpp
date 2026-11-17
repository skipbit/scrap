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
 * The reason @p fault gives when it reports an unusable toolchain, or
 * "<other fault>".
 */
std::string reasonOf(const InstalledToolchainFault& fault)
{
    const auto* unusable = std::get_if<UnusableInstalled>(&fault);
    return unusable == nullptr ? "<other fault>" : unusable->reason;
}

/**
 * The version of the toolchain @p specifier finds below @p home, or
 * "<none>" when it finds none.
 */
std::string versionFound(const TempDirectory& home, std::string_view specifier)
{
    const auto found = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier(specifier).value());
    EXPECT_TRUE(found.has_value()) << reasonOf(found.error());
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
    EXPECT_TRUE(reasonOf(broken.error()).ends_with("line 1: compiler: must be a string")) << reasonOf(broken.error());

    home.writeFile("toolchains/gcc/15.2.0/linux-x86_64/" + std::string{ InstallRecordName }, "compiler = \"bin/clang++\"\n");
    const auto missing = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("gcc").value());
    ASSERT_FALSE(missing.has_value());
    EXPECT_TRUE(reasonOf(missing.error()).ends_with("bin/clang++', which is not there")) << reasonOf(missing.error());
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
    ASSERT_TRUE(found.has_value()) << reasonOf(found.error());
    ASSERT_TRUE(found->has_value());
    const std::vector<std::filesystem::path> expected{ home.path() / directory / "lib64" };
    EXPECT_EQ((*found)->runtimeLibraryDirectories, expected);

    home.writeFile(directory + "/" + std::string{ InstallRecordName }, "compiler = \"bin/g++\"\nruntime-library-dirs = [\"lib\"]\n");
    const auto missing = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("gcc").value());
    ASSERT_FALSE(missing.has_value());
    EXPECT_TRUE(reasonOf(missing.error()).ends_with("/lib', which is not there")) << reasonOf(missing.error());
}

/**
 * The compile and link arguments the record names are taken as written.
 */
TEST(InstalledToolchainTest, FindsTheCompileAndLinkArguments)
{
    const TempDirectory home;
    install(home, "llvm", "22.1.8");
    const std::string directory = "toolchains/llvm/22.1.8/linux-x86_64";
    home.writeFile(directory + "/" + std::string{ InstallRecordName },
                   "compiler = 'bin/g++'\ncompile-args = ['-stdlib=libc++']\nlink-args = ['-stdlib=libc++', '-fuse-ld=lld']\n");

    const auto found = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("llvm").value());
    ASSERT_TRUE(found.has_value()) << reasonOf(found.error());
    ASSERT_TRUE(found->has_value());
    EXPECT_EQ((*found)->arguments.compile, std::vector<std::string>{ "-stdlib=libc++" });
    const std::vector<std::string> link{ "-stdlib=libc++", "-fuse-ld=lld" };
    EXPECT_EQ((*found)->arguments.link, link);
}

/**
 * A record written by an earlier scrap for a toolchain with libc++ says
 * nothing of what the compiler needs to build with it, so the toolchain is
 * installed again rather than built with in part.
 */
TEST(InstalledToolchainTest, RefusesARecordOfAnEarlierScrapForLibcxx)
{
    const TempDirectory home;
    install(home, "llvm", "22.1.8", "darwin-aarch64");
    const std::string directory = "toolchains/llvm/22.1.8/darwin-aarch64";
    home.writeFile(directory + "/lib/libc++.1.dylib", "");
    home.writeFile(directory + "/" + std::string{ InstallRecordName }, "compiler = 'bin/g++'\nruntime-library-dirs = ['lib']\n");
    const HostEnvironment darwin{ .platform = "darwin", .arch = "aarch64" };

    const auto old = findInstalledToolchain(home.path(), darwin, parseToolchainSpecifier("llvm").value());
    ASSERT_FALSE(old.has_value());
    EXPECT_NE(reasonOf(old.error()).find("was written by an earlier scrap"), std::string::npos) << reasonOf(old.error());
    EXPECT_TRUE(reasonOf(old.error()).ends_with("/lib'")) << reasonOf(old.error());

    home.writeFile(directory + "/" + std::string{ InstallRecordName }, "compiler = 'bin/g++'\nruntime-library-dirs = ['lib']\nlink-args = ['-lc++abi']\n");
    const auto current = findInstalledToolchain(home.path(), darwin, parseToolchainSpecifier("llvm").value());
    ASSERT_TRUE(current.has_value()) << reasonOf(current.error());
    EXPECT_TRUE(current->has_value());
}

/**
 * A record of GCC names no arguments, as its runtime libraries need none.
 */
TEST(InstalledToolchainTest, TakesARecordWithoutArgumentsForLibstdcxx)
{
    const TempDirectory home;
    install(home, "gcc", "15.2.0");
    const std::string directory = "toolchains/gcc/15.2.0/linux-x86_64";
    home.writeFile(directory + "/lib64/libstdc++.so.6", "");
    home.writeFile(directory + "/" + std::string{ InstallRecordName }, "compiler = 'bin/g++'\nruntime-library-dirs = ['lib64']\n");

    const auto found = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("gcc").value());
    ASSERT_TRUE(found.has_value()) << reasonOf(found.error());
    EXPECT_TRUE(found->has_value());
}

/**
 * A toolchain is named as the specifier that selects it is written.
 */
TEST(InstalledToolchainTest, NamesItselfAsFamilyAndVersion)
{
    const InstalledToolchain toolchain{
        .family = "gcc", .version = ToolchainVersion::parse("15.2.0").value(), .directory = "", .compiler = "", .runtimeLibraryDirectories = {}, .arguments = { .compile = {}, .link = {} }
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

/**
 * A family directory that cannot be listed is reported as such, with where
 * and why, rather than as a toolchain to install again.
 */
TEST(InstalledToolchainTest, ReportsAFamilyThatCannotBeListed)
{
    const TempDirectory home;
    home.writeFile("toolchains/.keep", "");
    // A link to itself fails to list even for a user every permission lets in.
    std::filesystem::create_directory_symlink("gcc", home.path() / "toolchains" / "gcc");

    const auto found = findInstalledToolchain(home.path(), Host, parseToolchainSpecifier("gcc").value());

    ASSERT_FALSE(found.has_value());
    const auto* unlisted = std::get_if<CannotListToolchains>(&found.error());
    ASSERT_NE(unlisted, nullptr) << reasonOf(found.error());
    EXPECT_EQ(unlisted->directory, home.path() / "toolchains" / "gcc");
    EXPECT_TRUE(unlisted->code) << unlisted->code.message();
}

TEST(InstalledToolchainTest, ReadsNoInstallRecordWhereThereIsNone)
{
    const TempDirectory directory;

    const auto read = usableInstallRecord(directory.path());

    ASSERT_TRUE(read.has_value()) << read.error();
    EXPECT_FALSE(read->has_value());
}

TEST(InstalledToolchainTest, ReadsTheInstallRecordWithItsIngotIntegrity)
{
    const TempDirectory directory;
    directory.writeFile("bin/g++", "");
    directory.writeFile(InstallRecordName, "compiler = \"bin/g++\"\ningot-integrity = \"sha256:abc\"\n");

    const auto read = usableInstallRecord(directory.path());

    ASSERT_TRUE(read.has_value()) << read.error();
    ASSERT_TRUE(read->has_value());
    EXPECT_EQ((*read)->ingotIntegrity, "sha256:abc");
}

TEST(InstalledToolchainTest, ReportsAnInstallRecordThatCannotBeParsedWithItsFile)
{
    const TempDirectory directory;
    directory.writeFile(InstallRecordName, "compiler = 1\n");

    const auto read = usableInstallRecord(directory.path());

    ASSERT_FALSE(read.has_value());
    EXPECT_TRUE(read.error().starts_with("'" + (directory.path() / InstallRecordName).string() + "': ")) << read.error();
}

/**
 * Only the record is read, so a compiler that is not there does not matter.
 */
TEST(InstalledToolchainTest, RefusesAnInstallRecordWhoseCompilerIsMissing)
{
    const TempDirectory directory;
    directory.writeFile(InstallRecordName, "compiler = \"bin/missing\"\n");

    const auto read = usableInstallRecord(directory.path());

    ASSERT_FALSE(read.has_value());
    EXPECT_NE(read.error().find("names the compiler"), std::string::npos) << read.error();
}

TEST(InstalledToolchainTest, RefusesAnInstallRecordWhoseRuntimeLibraryDirectoryIsMissing)
{
    const TempDirectory directory;
    directory.writeFile("bin/g++", "");
    directory.writeFile(InstallRecordName, "compiler = \"bin/g++\"\nruntime-library-dirs = [\"lib\"]\n");

    const auto read = usableInstallRecord(directory.path());

    ASSERT_FALSE(read.has_value());
    EXPECT_NE(read.error().find("names the runtime library directory"), std::string::npos) << read.error();
}

namespace {

/**
 * The names of the toolchains listed below @p home, or "<fault>" when the
 * listing fails.
 */
std::vector<std::string> namesListed(const TempDirectory& home)
{
    const auto listed = listInstalledToolchains(home.path(), Host);
    EXPECT_TRUE(listed.has_value());
    if (! listed.has_value()) {
        return { "<fault>" };
    }
    std::vector<std::string> names;
    for (const InstalledName& name : *listed) {
        names.push_back(name.text());
    }
    return names;
}

}  // namespace

/**
 * Without a toolchains directory, or with an empty one, nothing is listed.
 */
TEST(InstalledToolchainTest, ListsNothingWhenNoneIsInstalled)
{
    const TempDirectory home;
    EXPECT_TRUE(namesListed(home).empty());

    home.makeDirectory("toolchains");
    EXPECT_TRUE(namesListed(home).empty());
}

/**
 * The toolchains are listed by family, and within a family from the oldest
 * version, comparing versions as numbers.
 */
TEST(InstalledToolchainTest, ListsByFamilyAndThenFromTheOldestVersion)
{
    const TempDirectory home;
    install(home, "llvm", "22.1.0");
    install(home, "gcc", "16.2.0");
    install(home, "gcc", "9.1.0");
    install(home, "gcc", "15.2.0");

    EXPECT_EQ(namesListed(home), (std::vector<std::string>{ "gcc@9.1.0", "gcc@15.2.0", "gcc@16.2.0", "llvm@22.1.0" }));
}

/**
 * What finding a toolchain passes over is not listed either: a version
 * without an install record, one for another host, a directory whose name
 * is not a version, and one whose name is not a family.
 */
TEST(InstalledToolchainTest, ListsNothingThatIsNotInstalledForTheHost)
{
    const TempDirectory home;
    install(home, "gcc", "15.1.0");
    home.makeDirectory("toolchains/gcc/15.2.0/linux-x86_64/bin");
    install(home, "gcc", "15.3.0", "darwin-aarch64");
    install(home, "gcc", "staging");
    install(home, "Not A Family", "1.0.0");

    EXPECT_EQ(namesListed(home), (std::vector<std::string>{ "gcc@15.1.0" }));
}

/**
 * A toolchain whose record cannot be used is still listed: the listing does
 * not read the records.
 */
TEST(InstalledToolchainTest, ListsAToolchainWhoseRecordCannotBeUsed)
{
    const TempDirectory home;
    home.writeFile("toolchains/gcc/15.2.0/linux-x86_64/" + std::string{ InstallRecordName }, "not a record");

    EXPECT_EQ(namesListed(home), (std::vector<std::string>{ "gcc@15.2.0" }));
}

/**
 * A toolchains directory or a family directory that cannot be listed is
 * reported with where and why, rather than listed as holding nothing.
 */
TEST(InstalledToolchainTest, ReportsWhatCannotBeListedWhenListing)
{
    const TempDirectory looped;
    // A link to itself fails to list even for a user every permission lets in.
    std::filesystem::create_directory_symlink("toolchains", looped.path() / "toolchains");
    const auto whole = listInstalledToolchains(looped.path(), Host);
    ASSERT_FALSE(whole.has_value());
    EXPECT_EQ(whole.error().directory, looped.path() / "toolchains");
    EXPECT_TRUE(whole.error().code) << whole.error().code.message();

    const TempDirectory home;
    install(home, "llvm", "22.1.0");
    std::filesystem::create_directory_symlink("gcc", home.path() / "toolchains" / "gcc");
    const auto family = listInstalledToolchains(home.path(), Host);
    ASSERT_FALSE(family.has_value());
    EXPECT_EQ(family.error().directory, home.path() / "toolchains" / "gcc");
    EXPECT_TRUE(family.error().code) << family.error().code.message();
}

namespace {

/**
 * The names of the toolchains of @p installed that @p specifier names, in
 * the order kept.
 */
std::vector<std::string> keptBy(const std::vector<InstalledName>& installed, std::string_view specifier)
{
    std::vector<std::string> names;
    for (const InstalledName& name : namedBy(installed, parseToolchainSpecifier(specifier).value())) {
        names.push_back(name.text());
    }
    return names;
}

}  // namespace

/**
 * Every version the specifier names is kept in the order given, and nothing
 * of another family or version.
 */
TEST(InstalledToolchainTest, KeepsEveryVersionTheSpecifierNamesInOrder)
{
    const std::vector<InstalledName> installed{ { .family = "gcc", .version = ToolchainVersion::parse("15.2.0").value() },
                                                { .family = "gcc", .version = ToolchainVersion::parse("15.10.0").value() },
                                                { .family = "gcc", .version = ToolchainVersion::parse("16.2.0").value() },
                                                { .family = "llvm", .version = ToolchainVersion::parse("15.0.0").value() } };

    EXPECT_EQ(keptBy(installed, "gcc@15"), (std::vector<std::string>{ "gcc@15.2.0", "gcc@15.10.0" }));
    EXPECT_EQ(keptBy(installed, "gcc@16.2.0"), (std::vector<std::string>{ "gcc@16.2.0" }));
    EXPECT_EQ(keptBy(installed, "gcc"), (std::vector<std::string>{ "gcc@15.2.0", "gcc@15.10.0", "gcc@16.2.0" }));
}

/**
 * A specifier that names none keeps none.
 */
TEST(InstalledToolchainTest, KeepsNothingWhenTheSpecifierNamesNone)
{
    const std::vector<InstalledName> installed{ { .family = "gcc", .version = ToolchainVersion::parse("15.2.0").value() } };

    EXPECT_TRUE(keptBy(installed, "gcc@14").empty());
    EXPECT_TRUE(keptBy(installed, "llvm").empty());
    EXPECT_TRUE(keptBy({}, "gcc").empty());
}

/**
 * Of the listed toolchains, the newest the specifier names is the one a
 * build would use, comparing versions as numbers.
 */
TEST(InstalledToolchainTest, NamesTheNewestListedToolchainASpecifierNames)
{
    const std::vector<InstalledName> installed{ { .family = "gcc", .version = ToolchainVersion::parse("15.10.0").value() },
                                                { .family = "gcc", .version = ToolchainVersion::parse("15.2.0").value() },
                                                { .family = "gcc", .version = ToolchainVersion::parse("16.2.0").value() } };

    EXPECT_EQ(newestNamedBy(installed, parseToolchainSpecifier("gcc@15").value()), std::optional<std::string>{ "gcc@15.10.0" });
    EXPECT_EQ(newestNamedBy(installed, parseToolchainSpecifier("gcc@14").value()), std::nullopt);
}
