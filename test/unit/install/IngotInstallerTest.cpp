#include "install/IngotInstaller.h"

#include "digest/Sha256.h"
#include "support/HttpTestServer.h"
#include "support/StoredXz.h"
#include "support/TarArchiveBuilder.h"
#include "support/TempDirectory.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace scrap::Install;
using scrap::TestSupport::HttpTestResponse;
using scrap::TestSupport::HttpTestServer;
using scrap::TestSupport::storedXz;
using scrap::TestSupport::TempDirectory;
using scrap::TestTar::directoryEntry;
using scrap::TestTar::endMarker;
using scrap::TestTar::fileEntry;
using scrap::Toolchain::HostEnvironment;
using scrap::Toolchain::Ingot;
using scrap::Toolchain::InstallRecordName;
using scrap::Toolchain::parseInstallRecord;
using scrap::Toolchain::RegistrySource;
using scrap::Toolchain::SelectedIngot;
using scrap::Toolchain::ToolchainVersion;
using scrap::Toolchain::UrlSource;

namespace {

constexpr HostEnvironment TestHost{ .platform = "linux", .arch = "x86_64" };

std::string sha256Of(const std::string_view bytes)
{
    scrap::Digest::Sha256 digest;
    digest.update(bytes);
    return digest.hexDigest();
}

std::string ingotOf(std::string members)
{
    return storedXz(members + endMarker());
}

std::string gccIngot(const std::string_view compilerContent = "new compiler")
{
    return ingotOf(fileEntry("bin/g++", compilerContent, 0755) + fileEntry("lib64/libstdc++.so.6", "runtime"));
}

class IngotInstallerTest : public ::testing::Test {
protected:
    /**
     * Install @p ingot, served from a server of this machine, whose index
     * value is @p sha256.
     */
    auto install(const std::string& ingot, const std::string& sha256)
    {
        return install(HttpTestResponse{ .status = 200, .headers = {}, .body = ingot, .stalls = false }, sha256);
    }

    auto install(HttpTestResponse response, const std::string& sha256) -> decltype(installIngot(std::declval<const IngotInstallRequest&>()))
    {
        const HttpTestServer server({ { "/ingot", std::move(response) } });
        return installIngot(requestFor(UrlSource{ .url = server.url("/ingot"), .sha256 = sha256 }));
    }

    auto install(const std::string& ingot)
    {
        return install(ingot, sha256Of(ingot));
    }

    [[nodiscard]] IngotInstallRequest requestFor(std::variant<RegistrySource, UrlSource> source)
    {
        return { .scrapHome = _home.path(),
                 .selected = SelectedIngot{ .family = "gcc",
                                            .version = ToolchainVersion::parse("15.2.0").value(),
                                            .compiler = "g++",
                                            .ingot = Ingot{ .platform = "linux", .arch = "x86_64", .source = std::move(source), .glibcVersion = std::nullopt } },
                 .environment = TestHost,
                 .certificateBundle = std::nullopt,
                 .announce = [this](const std::string_view step) {
            _steps.emplace_back(step);
        } };
    }

    [[nodiscard]] std::filesystem::path destination() const
    {
        return _home.path() / "toolchains" / "gcc" / "15.2.0" / "linux-x86_64";
    }

    /** What was left to see of a failed install. */
    void expectNothingLeftBehind() const
    {
        EXPECT_FALSE(std::filesystem::exists(_home.path() / "toolchains"));
        EXPECT_TRUE(std::filesystem::is_empty(_home.path() / "tmp"));
    }

    TempDirectory _home;
    std::vector<std::string> _steps;
};

}  // anonymous namespace

TEST_F(IngotInstallerTest, PutsTheToolchainInItsPlace)
{
    const auto installed = install(gccIngot());

    ASSERT_TRUE(installed.has_value());
    EXPECT_EQ(*installed, destination());
    EXPECT_EQ(_home.readFile(destination() / "bin" / "g++"), "new compiler");
    EXPECT_TRUE(std::filesystem::is_empty(_home.path() / "tmp"));
}

TEST_F(IngotInstallerTest, InstallsWhatScrapThenFindsAndCanUse)
{
    ASSERT_TRUE(install(gccIngot()).has_value());

    const auto found = scrap::Toolchain::findInstalledToolchain(_home.path(), TestHost, scrap::Toolchain::parseToolchainSpecifier("gcc").value());

    ASSERT_TRUE(found.has_value());
    ASSERT_TRUE(found->has_value());
    EXPECT_EQ((*found)->compiler, destination() / "bin" / "g++");
    EXPECT_EQ((*found)->runtimeLibraryDirectories, (std::vector<std::filesystem::path>{ destination() / "lib64" }));
}

TEST_F(IngotInstallerTest, RecordsWhatTheIndexGaveForIt)
{
    const std::string ingot = gccIngot();
    ASSERT_TRUE(install(ingot).has_value());

    const auto record = parseInstallRecord(_home.readFile(destination() / InstallRecordName));

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->compiler, std::filesystem::path{ "bin/g++" });
    EXPECT_EQ(record->ingotIntegrity, sha256Of(ingot));
}

TEST_F(IngotInstallerTest, TakesTheToolchainOutOfTheOneDirectoryItCameIn)
{
    const auto installed = install(ingotOf(directoryEntry("LLVM-20/") + fileEntry("LLVM-20/bin/g++", "compiler", 0755)));

    ASSERT_TRUE(installed.has_value());
    EXPECT_EQ(_home.readFile(destination() / "bin" / "g++"), "compiler");
    EXPECT_FALSE(std::filesystem::exists(destination() / "LLVM-20"));
}

TEST_F(IngotInstallerTest, FindsRuntimeLibrariesWhereverTheyAreBelowLib)
{
    const std::string ingot = ingotOf(fileEntry("bin/g++", "compiler", 0755) + fileEntry("lib/x86_64-unknown-linux-gnu/libc++.so.1", "runtime")
                                      + fileEntry("lib/x86_64-unknown-linux-gnu/libc++.so", "runtime") + fileEntry("lib/other/libm.so", "unrelated"));
    ASSERT_TRUE(install(ingot).has_value());

    const auto record = parseInstallRecord(_home.readFile(destination() / InstallRecordName));

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->runtimeLibraryDirectories, (std::vector<std::filesystem::path>{ "lib/x86_64-unknown-linux-gnu" }));
}

TEST_F(IngotInstallerTest, RecognisesTheRuntimeLibraryOfMacOs)
{
    ASSERT_TRUE(install(ingotOf(fileEntry("bin/g++", "compiler", 0755) + fileEntry("lib/libc++.1.dylib", "runtime"))).has_value());

    const auto record = parseInstallRecord(_home.readFile(destination() / InstallRecordName));

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->runtimeLibraryDirectories, (std::vector<std::filesystem::path>{ "lib" }));
}

TEST_F(IngotInstallerTest, SaysWhatItIsDoing)
{
    ASSERT_TRUE(install(gccIngot()).has_value());

    EXPECT_EQ(_steps, (std::vector<std::string>{ "Downloading 'gcc@15.2.0'", "Extracting 'gcc@15.2.0'" }));
}

TEST_F(IngotInstallerTest, RefusesAnIngotThatDoesNotMatchTheIndexBeforeExtracting)
{
    const auto installed = install(gccIngot(), sha256Of("something else"));

    ASSERT_FALSE(installed.has_value());
    EXPECT_TRUE(std::holds_alternative<IngotDoesNotMatch>(installed.error()));
    EXPECT_EQ(_steps, (std::vector<std::string>{ "Downloading 'gcc@15.2.0'" }));
    expectNothingLeftBehind();
}

TEST_F(IngotInstallerTest, LeavesNothingWhenDecodingFailsAfterMembersWereExtracted)
{
    // The first member is whole in the first chunk, so it is written before the
    // file ends too soon.
    const std::string whole = ingotOf(fileEntry("bin/g++", "compiler", 0755) + fileEntry("data", std::string(200000, 'x')));
    const std::string cutShort = whole.substr(0, 100000);

    const auto installed = install(cutShort);

    ASSERT_FALSE(installed.has_value());
    const auto* failure = std::get_if<IngotNotUnpacked>(&installed.error());
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->reason, "the ingot ends too soon");
    expectNothingLeftBehind();
}

TEST_F(IngotInstallerTest, LeavesNothingWhenTheArchiveHasAnUnsafeMember)
{
    const auto installed = install(ingotOf(fileEntry("bin/g++", "compiler", 0755) + fileEntry("../outside", "x")));

    ASSERT_FALSE(installed.has_value());
    const auto* failure = std::get_if<IngotNotUnpacked>(&installed.error());
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->reason, "the archive holds the unsafe path '../outside'");
    expectNothingLeftBehind();
}

TEST_F(IngotInstallerTest, LeavesNothingWhenTheArchiveHasNoEndMarker)
{
    const auto installed = install(storedXz(fileEntry("bin/g++", "compiler", 0755)));

    ASSERT_FALSE(installed.has_value());
    EXPECT_TRUE(std::holds_alternative<IngotNotUnpacked>(installed.error()));
    expectNothingLeftBehind();
}

TEST_F(IngotInstallerTest, LeavesNothingWhenTheIngotIsNotXz)
{
    const auto installed = install(std::string{ "not an ingot" });

    ASSERT_FALSE(installed.has_value());
    const auto* failure = std::get_if<IngotNotUnpacked>(&installed.error());
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->reason, "the ingot is not an xz file");
    expectNothingLeftBehind();
}

TEST_F(IngotInstallerTest, RefusesAnIngotWithoutTheCompilerTheIndexNames)
{
    const auto installed = install(ingotOf(fileEntry("bin/clang++", "compiler", 0755)));

    ASSERT_FALSE(installed.has_value());
    const auto* failure = std::get_if<IngotNotUnpacked>(&installed.error());
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->reason, "the ingot holds no 'bin/g++'");
    expectNothingLeftBehind();
}

TEST_F(IngotInstallerTest, RefusesAnIngotThatBringsItsOwnInstallRecord)
{
    const auto installed = install(ingotOf(fileEntry("bin/g++", "compiler", 0755) + fileEntry(InstallRecordName, "compiler = 'bin/g++'\n")));

    ASSERT_FALSE(installed.has_value());
    EXPECT_FALSE(std::filesystem::exists(_home.path() / "toolchains"));
}

TEST_F(IngotInstallerTest, ReportsAFailedFetchAndLeavesNothing)
{
    const auto installed = install(HttpTestResponse{ .status = 404, .headers = {}, .body = {}, .stalls = false }, "unused");

    ASSERT_FALSE(installed.has_value());
    EXPECT_TRUE(std::holds_alternative<IngotNotFetched>(installed.error()));
    expectNothingLeftBehind();
}

TEST_F(IngotInstallerTest, LeavesNothingWhenTheTransferIsCutShort)
{
    const auto installed = install(HttpTestResponse{ .status = 200, .headers = {}, .body = "part", .stalls = false, .bytesCutOff = 100 }, "unused");

    ASSERT_FALSE(installed.has_value());
    EXPECT_TRUE(std::holds_alternative<IngotNotFetched>(installed.error()));
    expectNothingLeftBehind();
}

TEST_F(IngotInstallerTest, ReplacesWhatIsInstalled)
{
    ASSERT_TRUE(install(gccIngot("old compiler")).has_value());
    _home.writeFile(std::filesystem::relative(destination() / "only-in-old", _home.path()).string(), "x");

    ASSERT_TRUE(install(gccIngot("new compiler")).has_value());

    EXPECT_EQ(_home.readFile(destination() / "bin" / "g++"), "new compiler");
    EXPECT_FALSE(std::filesystem::exists(destination() / "only-in-old"));
    EXPECT_TRUE(std::filesystem::is_empty(_home.path() / "tmp"));
}

TEST_F(IngotInstallerTest, KeepsWhatIsInstalledWhenReplacingItFails)
{
    const std::string first = gccIngot("old compiler");
    ASSERT_TRUE(install(first).has_value());

    ASSERT_FALSE(install(ingotOf(fileEntry("bin/other", "compiler"))).has_value());

    EXPECT_EQ(_home.readFile(destination() / "bin" / "g++"), "old compiler");
    EXPECT_EQ(parseInstallRecord(_home.readFile(destination() / InstallRecordName))->ingotIntegrity, sha256Of(first));
}

TEST_F(IngotInstallerTest, DoesNotYetFetchFromARegistry)
{
    const auto installed = installIngot(requestFor(RegistrySource{ .registry = "ghcr.io/example", .tag = "gcc:15.2.0", .digest = "sha256:abc" }));

    ASSERT_FALSE(installed.has_value());
    EXPECT_TRUE(std::holds_alternative<RegistryNotSupported>(installed.error()));
    EXPECT_FALSE(std::filesystem::exists(_home.path() / "toolchains"));
}

TEST_F(IngotInstallerTest, ClearsWorkAbandonedByAnEarlierRun)
{
    const std::filesystem::path abandoned = _home.makeDirectory("tmp/install-old");
    std::filesystem::last_write_time(abandoned, std::filesystem::file_time_type::clock::now() - std::chrono::hours{ 25 });

    ASSERT_TRUE(install(gccIngot()).has_value());

    EXPECT_FALSE(std::filesystem::exists(abandoned));
}

TEST(DiscardAbandonedWorkTest, RemovesOnlyWhatIsADayOld)
{
    const TempDirectory work;
    const auto now = std::filesystem::file_time_type::clock::now();
    const std::filesystem::path old = work.makeDirectory("old");
    work.writeFile("old/nested/file", "x");
    const std::filesystem::path young = work.makeDirectory("young");
    std::filesystem::last_write_time(old, now - std::chrono::hours{ 24 });
    std::filesystem::last_write_time(young, now - std::chrono::hours{ 23 });

    discardAbandonedWork(work.path(), now);

    EXPECT_FALSE(std::filesystem::exists(old));
    EXPECT_TRUE(std::filesystem::exists(young));
}

TEST(DiscardAbandonedWorkTest, PassesOverADirectoryThatIsNotThere)
{
    const TempDirectory work;

    discardAbandonedWork(work.path() / "missing", std::filesystem::file_time_type::clock::now());

    EXPECT_FALSE(std::filesystem::exists(work.path() / "missing"));
}
