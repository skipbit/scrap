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

// 2026-11-15T10:00:00Z.
const std::chrono::system_clock::time_point FetchTime{ std::chrono::seconds{ 1794736800 } };

// 2021-01-01T00:00:00Z.
const std::chrono::sys_seconds EarlierTime{ std::chrono::seconds{ 1609459200 } };

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
    auto obtainFrom(HttpTestServer& server, HttpTestResponse response)
    {
        server.respond("/index.toml", std::move(response));
        return obtainIndex(_home.path(), IndexSource{ .url = server.url("/index.toml"), .certificateBundle = std::nullopt }, ToolchainVersion::parse("1.0.0").value(), FetchTime);
    }

    auto fetchFrom(HttpTestServer& server, HttpTestResponse response)
    {
        server.respond("/index.toml", std::move(response));
        return fetchIndex(_home.path(), IndexSource{ .url = server.url("/index.toml"), .certificateBundle = std::nullopt }, ToolchainVersion::parse("1.0.0").value(), FetchTime);
    }

    auto fetch(HttpTestResponse response)
    {
        return fetchFrom(_server, std::move(response));
    }

    auto obtain(HttpTestResponse response)
    {
        return obtainFrom(_server, std::move(response));
    }

    static std::string versionIn(const ObtainedIndex& obtained)
    {
        return obtained.index.toolchains.at(0).version.text();
    }

    HttpTestServer _server{ {} };
    TempDirectory _home;
};

}  // anonymous namespace

TEST_F(IndexCopyTest, FetchesTheIndexAndSavesIt)
{
    const auto fetched = fetch(answer(indexOf("15.2.0")));

    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->index.toolchains.at(0).version.text(), "15.2.0");
    EXPECT_EQ(fetched->fetchedAt, "2026-11-15T10:00:00Z");
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopyFile(_home.path())), indexOf("15.2.0"));
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopySourceFile(_home.path())), _server.url("/index.toml") + "\n2026-11-15T10:00:00Z\n");
}

TEST_F(IndexCopyTest, DoesNotFallBackOnTheSavedCopyWhenTheFetchFails)
{
    ASSERT_TRUE(fetch(answer(indexOf("15.1.0"))).has_value());

    const auto fetched = fetch(HttpTestResponse{ .status = 500, .headers = {}, .body = {}, .stalls = false });

    ASSERT_FALSE(fetched.has_value());
    EXPECT_TRUE(std::holds_alternative<IndexUnavailable>(fetched.error()));
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopyFile(_home.path())), indexOf("15.1.0"));
}

TEST_F(IndexCopyTest, FetchReportsTheTransferThatFailed)
{
    const auto fetched = fetch(HttpTestResponse{ .status = 500, .headers = {}, .body = {}, .stalls = false });

    ASSERT_FALSE(fetched.has_value());
    const auto& failure = std::get<IndexUnavailable>(fetched.error());
    EXPECT_NE(failure.reason.find("status 500"), std::string::npos);
    ASSERT_TRUE(failure.fetchError.has_value());
    EXPECT_TRUE(std::holds_alternative<scrap::Fetch::UnexpectedStatus>(*failure.fetchError));
}

TEST_F(IndexCopyTest, ReadsWhatItFetchesAndSavesIt)
{
    const auto obtained = obtain(answer(indexOf("15.2.0")));

    ASSERT_TRUE(obtained.has_value());
    EXPECT_EQ(versionIn(*obtained), "15.2.0");
    EXPECT_FALSE(obtained->standIn.has_value());
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

    const auto obtained = obtain(HttpTestResponse{ .status = 500, .headers = {}, .body = {}, .stalls = false });

    ASSERT_TRUE(obtained.has_value());
    EXPECT_EQ(versionIn(*obtained), "15.1.0");
    ASSERT_TRUE(obtained->standIn.has_value());
    EXPECT_EQ(obtained->standIn->fetchedAt, "2026-11-15T10:00:00Z");
    EXPECT_NE(obtained->standIn->reason.find("status 500"), std::string::npos) << obtained->standIn->reason;
}

/**
 * An earlier scrap recorded only the URL, and the copy was last written when
 * it was fetched.
 */
TEST_F(IndexCopyTest, SaysWhenACopyAnEarlierScrapSavedWasLastWritten)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());
    _home.writeFile(std::filesystem::relative(scrap::Toolchain::indexCopySourceFile(_home.path()), _home.path()).string(), _server.url("/index.toml") + "\n");
    std::filesystem::last_write_time(scrap::Toolchain::indexCopyFile(_home.path()), std::chrono::file_clock::from_sys(EarlierTime));

    const auto obtained = obtain(HttpTestResponse{ .status = 500, .headers = {}, .body = {}, .stalls = false });

    ASSERT_TRUE(obtained.has_value());
    ASSERT_TRUE(obtained->standIn.has_value());
    EXPECT_EQ(obtained->standIn->fetchedAt, "2021-01-01T00:00:00Z");
}

TEST_F(IndexCopyTest, RecordsWhereAndWhenTheSavedCopyWasFetched)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());

    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopySourceFile(_home.path())), _server.url("/index.toml") + "\n2026-11-15T10:00:00Z\n");
}

/**
 * One index must not answer for another, whose URLs and digests it would bring.
 */
TEST_F(IndexCopyTest, DoesNotStandInWithTheCopyOfAnotherIndex)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());
    HttpTestServer other{ {} };

    const auto obtained = obtainFrom(other, HttpTestResponse{ .status = 500, .headers = {}, .body = {}, .stalls = false });

    ASSERT_FALSE(obtained.has_value());
    EXPECT_NE(std::get<IndexUnavailable>(obtained.error()).reason.find("status 500"), std::string::npos);
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopyFile(_home.path())), indexOf("15.1.0"));
}

TEST_F(IndexCopyTest, DoesNotStandInWithACopyNothingSaysTheSourceOf)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());
    ASSERT_TRUE(std::filesystem::remove(scrap::Toolchain::indexCopySourceFile(_home.path())));

    const auto obtained = obtain(HttpTestResponse{ .status = 500, .headers = {}, .body = {}, .stalls = false });

    EXPECT_FALSE(obtained.has_value());
}

TEST_F(IndexCopyTest, TheLatestIndexFetchedIsTheOneThatStandsIn)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());
    HttpTestServer other{ {} };
    ASSERT_TRUE(obtainFrom(other, answer(indexOf("15.2.0"))).has_value());

    const auto obtained = obtainFrom(other, HttpTestResponse{ .status = 500, .headers = {}, .body = {}, .stalls = false });

    ASSERT_TRUE(obtained.has_value());
    EXPECT_EQ(versionIn(*obtained), "15.2.0");
}

TEST_F(IndexCopyTest, KeepsTheSavedCopyWhenATransferIsCutShort)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());

    const auto obtained = obtain(HttpTestResponse{ .status = 200, .headers = {}, .body = "[index]\n", .stalls = false, .bytesCutOff = 1000 });

    ASSERT_TRUE(obtained.has_value());
    EXPECT_TRUE(obtained->standIn.has_value());
    EXPECT_EQ(_home.readFile(scrap::Toolchain::indexCopyFile(_home.path())), indexOf("15.1.0"));
}

TEST_F(IndexCopyTest, KeepsTheSavedCopyWhenWhatIsFetchedCannotBeRead)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());

    const auto obtained = obtain(answer("this is not an index"));

    ASSERT_TRUE(obtained.has_value());
    EXPECT_EQ(versionIn(*obtained), "15.1.0");
    EXPECT_TRUE(obtained->standIn.has_value());
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

TEST_F(IndexCopyTest, ReadsTheSavedCopyAndWhenItWasFetched)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());

    const auto saved = readSavedIndex(_home.path(), _server.url("/index.toml"), ToolchainVersion::parse("1.0.0").value());

    ASSERT_TRUE(saved.has_value());
    ASSERT_TRUE(saved->has_value());
    EXPECT_EQ((*saved)->index.toolchains.at(0).version.text(), "15.1.0");
    EXPECT_EQ((*saved)->fetchedAt, "2026-11-15T10:00:00Z");
}

TEST_F(IndexCopyTest, ReadsNoCopyWhenNoneIsSaved)
{
    const auto saved = readSavedIndex(_home.path(), _server.url("/index.toml"), ToolchainVersion::parse("1.0.0").value());

    ASSERT_TRUE(saved.has_value());
    EXPECT_FALSE(saved->has_value());
}

TEST_F(IndexCopyTest, ReadsNoCopyOfAnotherIndex)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());
    HttpTestServer other{ {} };

    const auto saved = readSavedIndex(_home.path(), other.url("/index.toml"), ToolchainVersion::parse("1.0.0").value());

    ASSERT_TRUE(saved.has_value());
    EXPECT_FALSE(saved->has_value());
}

TEST_F(IndexCopyTest, ReadsNoCopyThatCannotBeRead)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());
    _home.writeFile(std::filesystem::relative(scrap::Toolchain::indexCopyFile(_home.path()), _home.path()).string(), "this is not an index");

    const auto saved = readSavedIndex(_home.path(), _server.url("/index.toml"), ToolchainVersion::parse("1.0.0").value());

    ASSERT_TRUE(saved.has_value());
    EXPECT_FALSE(saved->has_value());
}

TEST_F(IndexCopyTest, DoesNotHideASavedCopyInAFormatThisScrapCannotRead)
{
    ASSERT_TRUE(obtain(answer(indexOf("15.1.0"))).has_value());
    _home.writeFile(std::filesystem::relative(scrap::Toolchain::indexCopyFile(_home.path()), _home.path()).string(), "[index]\nschema_version = 2\nchannel = 'official'\n");

    const auto saved = readSavedIndex(_home.path(), _server.url("/index.toml"), ToolchainVersion::parse("1.0.0").value());

    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().format, "2");
}
