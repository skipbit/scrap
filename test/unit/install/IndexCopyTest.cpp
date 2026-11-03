#include "install/IndexCopy.h"

#include "fetch/UrlFetch.h"
#include "support/HttpTestServer.h"
#include "support/TempDirectory.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>

using namespace scrap::Install;
using scrap::TestSupport::HttpTestResponse;
using scrap::TestSupport::HttpTestServer;
using scrap::TestSupport::TempDirectory;
using scrap::Toolchain::ToolchainVersion;

namespace {

std::string indexOf(const std::string& version)
{
    return "[index]\nschema_version = 1\nchannel = 'official'\n\n[[toolchains]]\nfamily = 'gcc'\nversion = '" + version + "'\nstatus = 'active'\ncompiler = 'g++'\nlicense = 'GPL-3.0-or-later'\n";
}

HttpTestResponse answer(std::string body)
{
    return { .status = 200, .headers = {}, .body = std::move(body), .stalls = false };
}

class IndexCopyTest : public ::testing::Test {
protected:
    auto obtain(HttpTestResponse response)
    {
        const HttpTestServer server({ { "/index.toml", std::move(response) } });
        return obtainIndex(_home.path(), IndexSource{ .url = server.url("/index.toml"), .certificateBundle = std::nullopt }, ToolchainVersion::parse("1.0.0").value());
    }

    static std::string versionIn(const ObtainedIndex& obtained)
    {
        return obtained.index.toolchains.at(0).version.text();
    }

    TempDirectory _home;
};

}  // anonymous namespace

TEST_F(IndexCopyTest, ReadsWhatItFetchesAndSavesIt)
{
    const auto obtained = obtain(answer(indexOf("15.2.0")));

    ASSERT_TRUE(obtained.has_value());
    EXPECT_EQ(versionIn(*obtained), "15.2.0");
    EXPECT_FALSE(obtained->copyFetchedAt.has_value());
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopyFile(_home.path())), indexOf("15.2.0"));
}

TEST_F(IndexCopyTest, ReplacesTheSavedCopyWithWhatIsNew)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());

    const auto obtained = obtain(answer(indexOf("15.2.0")));

    ASSERT_TRUE(obtained.has_value());
    EXPECT_EQ(versionIn(*obtained), "15.2.0");
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopyFile(_home.path())), indexOf("15.2.0"));
}

TEST_F(IndexCopyTest, FallsBackOnTheSavedCopyAndSaysWhenItWasFetched)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());
    const auto copy = scrap::Toolchain::indexCopyFile(_home.path());
    // 2021-01-01T00:00:00Z.
    std::filesystem::last_write_time(copy, std::chrono::file_clock::from_sys(std::chrono::sys_seconds{ std::chrono::seconds{ 1609459200 } }));

    const auto obtained = obtain(HttpTestResponse{ .status = 500, .headers = {}, .body = {}, .stalls = false });

    ASSERT_TRUE(obtained.has_value());
    EXPECT_EQ(versionIn(*obtained), "15.1.0");
    EXPECT_EQ(obtained->copyFetchedAt, "2021-01-01T00:00:00Z");
}

TEST_F(IndexCopyTest, KeepsTheSavedCopyWhenATransferIsCutShort)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());

    const auto obtained = obtain(HttpTestResponse{ .status = 200, .headers = {}, .body = "[index]\n", .stalls = false, .bytesCutOff = 1000 });

    ASSERT_TRUE(obtained.has_value());
    EXPECT_TRUE(obtained->copyFetchedAt.has_value());
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopyFile(_home.path())), indexOf("15.1.0"));
}

TEST_F(IndexCopyTest, KeepsTheSavedCopyWhenWhatIsFetchedCannotBeRead)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());

    const auto obtained = obtain(answer("this is not an index"));

    ASSERT_TRUE(obtained.has_value());
    EXPECT_EQ(versionIn(*obtained), "15.1.0");
    EXPECT_TRUE(obtained->copyFetchedAt.has_value());
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopyFile(_home.path())), indexOf("15.1.0"));
}

TEST_F(IndexCopyTest, ReportsTheTransferWhenThereIsNoSavedCopy)
{
    const auto obtained = obtain(HttpTestResponse{ .status = 500, .headers = {}, .body = {}, .stalls = false });

    ASSERT_FALSE(obtained.has_value());
    const auto& failure = std::get<IndexUnavailable>(obtained.error());
    EXPECT_NE(failure.reason.find("status 500"), std::string::npos);
    ASSERT_TRUE(failure.fetchError.has_value());
    EXPECT_TRUE(std::holds_alternative<scrap::Fetch::UnexpectedStatus>(*failure.fetchError));
}

TEST_F(IndexCopyTest, ReportsAnUnreadableIndexWhenThereIsNoSavedCopy)
{
    const auto obtained = obtain(answer("this is not an index"));

    ASSERT_FALSE(obtained.has_value());
    const auto& failure = std::get<IndexUnavailable>(obtained.error());
    EXPECT_FALSE(failure.reason.empty());
    EXPECT_FALSE(failure.fetchError.has_value());
    EXPECT_FALSE(std::filesystem::exists(scrap::Toolchain::indexCopyFile(_home.path())));
}

TEST_F(IndexCopyTest, DoesNotHideAFormatThisScrapCannotRead)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());

    const auto obtained = obtain(answer("[index]\nschema_version = 2\nchannel = 'official'\n"));

    ASSERT_FALSE(obtained.has_value());
    EXPECT_EQ(std::get<IndexFormatUnsupported>(obtained.error()).format, "2");
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopyFile(_home.path())), indexOf("15.1.0"));
}

TEST_F(IndexCopyTest, RefusesAnIndexPastTheSizeLimit)
{
    const std::size_t pastLimit = (std::size_t{ 16 } * 1024 * 1024) + 1;

    const auto obtained = obtain(answer(std::string(pastLimit, '#')));

    ASSERT_FALSE(obtained.has_value());
    const auto& failure = std::get<IndexUnavailable>(obtained.error());
    ASSERT_TRUE(failure.fetchError.has_value());
    EXPECT_TRUE(std::holds_alternative<scrap::Fetch::ContentTooLarge>(*failure.fetchError));
}
