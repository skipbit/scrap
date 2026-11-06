#include "fetch/UrlFetch.h"

#include "file/WholeFile.h"
#include "support/HttpTestServer.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

using namespace scrap::Fetch;
using scrap::File::readWholeFile;
using scrap::File::writeNewFile;
using scrap::TestSupport::HttpTestResponse;
using scrap::TestSupport::HttpTestServer;
using scrap::TestSupport::TempDirectory;

namespace {

HttpTestResponse content(std::string body)
{
    return { .status = 200, .headers = {}, .body = std::move(body), .stalls = false };
}

HttpTestResponse redirectTo(const std::string& location, const int status = 302)
{
    return { .status = status, .headers = "Location: " + location + "\r\n", .body = "redirect body", .stalls = false };
}

FetchRequest requestFor(std::string url, std::filesystem::path destination)
{
    return { .url = std::move(url), .destination = std::move(destination), .certificateBundle = std::nullopt, .sizeLimit = std::nullopt, .headers = {} };
}

/**
 * Set an environment variable for a test's lifetime, and put it back.
 */
class ScopedVariable {
public:
    ScopedVariable(std::string name, const std::string& value)
        : _name(std::move(name))
    {
        if (const char* previous = std::getenv(_name.c_str()); previous != nullptr) {
            _previous = previous;
        }
        ::setenv(_name.c_str(), value.c_str(), 1);
    }
    ScopedVariable(const ScopedVariable&) = delete;
    ScopedVariable& operator=(const ScopedVariable&) = delete;
    ScopedVariable(ScopedVariable&&) = delete;
    ScopedVariable& operator=(ScopedVariable&&) = delete;
    ~ScopedVariable()
    {
        if (_previous.has_value()) {
            ::setenv(_name.c_str(), _previous->c_str(), 1);
        } else {
            ::unsetenv(_name.c_str());
        }
    }

private:
    std::string _name;
    std::optional<std::string> _previous;
};

}  // anonymous namespace

TEST(UrlFetchTest, PutsTheBytesOfTheResponseAtTheDestination)
{
    const TempDirectory temp;
    std::string body(3U * 1024U * 1024U, 'x');
    body[0] = '\0';
    body.back() = '\xff';
    const HttpTestServer server{ { { "/file", content(body) } } };

    const auto fetched = fetchToFile(requestFor(server.url("/file"), temp.path() / "out"));

    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(readWholeFile(temp.path() / "out"), body);
    EXPECT_EQ(temp.entries(), (std::vector<std::string>{ "out" }));
}

TEST(UrlFetchTest, PutsAnEmptyFileForAnEmptyResponse)
{
    const TempDirectory temp;
    const HttpTestServer server{ { { "/empty", content("") } } };

    ASSERT_TRUE(fetchToFile(requestFor(server.url("/empty"), temp.path() / "out")).has_value());

    EXPECT_EQ(readWholeFile(temp.path() / "out"), "");
}

TEST(UrlFetchTest, AcceptsLocalhostByName)
{
    const TempDirectory temp;
    const HttpTestServer server{ { { "/file", content("by name") } } };
    const std::string url = "http://localhost:" + std::to_string(server.port()) + "/file";

    ASSERT_TRUE(fetchToFile(requestFor(url, temp.path() / "out")).has_value());

    EXPECT_EQ(readWholeFile(temp.path() / "out"), "by name");
}

/**
 * Nothing listens on port 1, so the address reaches the transfer: it was not
 * refused for what it says.
 */
TEST(UrlFetchTest, AcceptsTheLoopbackAddressOfIpv6)
{
    const TempDirectory temp;

    const auto fetched = fetchToFile(requestFor("http://[::1]:1/file", temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    EXPECT_TRUE(std::holds_alternative<TransferFailure>(fetched.error()));
}

TEST(UrlFetchTest, RefusesHttpToAnotherHost)
{
    const TempDirectory temp;

    const auto fetched = fetchToFile(requestFor("http://example.com/file", temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    const auto* refused = std::get_if<UrlRefused>(&fetched.error());
    ASSERT_NE(refused, nullptr);
    EXPECT_EQ(refused->url, "http://example.com/file");
}

/**
 * Each of these names another host, however much of it reads as this machine.
 */
TEST(UrlFetchTest, RefusesHttpThatOnlyLooksLikeThisMachine)
{
    const TempDirectory temp;
    for (const char* url : {
             "http://127.0.0.1@example.com/file",
             "http://localhost.example.com/file",
             "http://127.0.0.1.example.com/file",
             "http://127.0.0.2/file",
             "http://example.com/127.0.0.1",
         }) {
        const auto fetched = fetchToFile(requestFor(url, temp.path() / "out"));

        ASSERT_FALSE(fetched.has_value()) << url;
        EXPECT_TRUE(std::holds_alternative<UrlRefused>(fetched.error())) << url;
    }
}

TEST(UrlFetchTest, RefusesOtherSchemesAndWhatIsNoUrl)
{
    const TempDirectory temp;
    for (const char* url : { "ftp://example.com/file", "file:///etc/hostname", "not a url", "" }) {
        const auto fetched = fetchToFile(requestFor(url, temp.path() / "out"));

        ASSERT_FALSE(fetched.has_value()) << url;
        EXPECT_TRUE(std::holds_alternative<UrlRefused>(fetched.error())) << url;
    }
}

TEST(UrlFetchTest, AsksForCertificatesBeforeAnyHttpsTransfer)
{
    const TempDirectory temp;

    const auto fetched = fetchToFile(requestFor("https://example.com/file", temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    EXPECT_TRUE(std::holds_alternative<NoCertificateBundle>(fetched.error()));
}

TEST(UrlFetchTest, ReportsCertificatesThatCannotBeRead)
{
    const TempDirectory temp;
    const HttpTestServer server{ {} };
    FetchRequest request = requestFor("https://127.0.0.1:" + std::to_string(server.port()) + "/file", temp.path() / "out");
    request.certificateBundle = temp.path() / "missing.pem";

    const auto fetched = fetchToFile(request);

    ASSERT_FALSE(fetched.has_value());
    EXPECT_TRUE(std::holds_alternative<CertificateFailure>(fetched.error()));
}

TEST(UrlFetchTest, ReportsAnErrorStatusAndLeavesTheDestinationAsItWas)
{
    const TempDirectory temp;
    const HttpTestServer server{ {} };
    ASSERT_FALSE(writeNewFile(temp.path() / "out", "previous").operator bool());

    const auto fetched = fetchToFile(requestFor(server.url("/nothing"), temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    const auto* status = std::get_if<UnexpectedStatus>(&fetched.error());
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status, 404);
    EXPECT_EQ(status->url, server.url("/nothing"));
    EXPECT_EQ(readWholeFile(temp.path() / "out"), "previous");
    EXPECT_EQ(temp.entries(), (std::vector<std::string>{ "out" }));
}

TEST(UrlFetchTest, ReportsAStatusThatIsNotTheContent)
{
    const TempDirectory temp;
    const HttpTestServer server{ { { "/partial", { .status = 206, .headers = {}, .body = "part", .stalls = false } } } };

    const auto fetched = fetchToFile(requestFor(server.url("/partial"), temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    const auto* status = std::get_if<UnexpectedStatus>(&fetched.error());
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status, 206);
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "out"));
}

TEST(UrlFetchTest, ReportsARedirectThatSaysNowhere)
{
    const TempDirectory temp;
    const HttpTestServer server{ { { "/nowhere", { .status = 302, .headers = {}, .body = {}, .stalls = false } } } };

    const auto fetched = fetchToFile(requestFor(server.url("/nowhere"), temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    const auto* status = std::get_if<UnexpectedStatus>(&fetched.error());
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status, 302);
}

TEST(UrlFetchTest, FollowsRedirectsAndKeepsOnlyTheLastBody)
{
    const TempDirectory temp;
    const HttpTestServer server{ {
        { "/first", redirectTo("/second", 301) },
        { "/second", redirectTo("third", 307) },
        { "/third", content("arrived") },
    } };

    ASSERT_TRUE(fetchToFile(requestFor(server.url("/first"), temp.path() / "out")).has_value());

    EXPECT_EQ(readWholeFile(temp.path() / "out"), "arrived");
    EXPECT_EQ(server.requestLines(), (std::vector<std::string>{ "GET /first HTTP/1.1", "GET /second HTTP/1.1", "GET /third HTTP/1.1" }));
}

TEST(UrlFetchTest, SendsTheHeadersToTheServerOfTheUrl)
{
    const TempDirectory temp;
    const HttpTestServer server{ {
        { "/first", redirectTo("/second") },
        { "/second", content("arrived") },
    } };
    FetchRequest request = requestFor(server.url("/first"), temp.path() / "out");
    request.headers = { "Authorization: Bearer secret", "Accept: text/plain" };

    ASSERT_TRUE(fetchToFile(request).has_value());

    const std::vector<std::string> heads = server.requestHeads();
    ASSERT_EQ(heads.size(), 2U);
    for (const std::string& head : heads) {
        EXPECT_NE(head.find("\r\nAuthorization: Bearer secret\r\n"), std::string::npos) << head;
        EXPECT_NE(head.find("\r\nAccept: text/plain\r\n"), std::string::npos) << head;
    }
}

TEST(UrlFetchTest, KeepsTheHeadersFromAnotherServerARedirectLeadsTo)
{
    const TempDirectory temp;
    HttpTestServer server{ {} };
    server.respond("/first", redirectTo("http://localhost:" + std::to_string(server.port()) + "/second"));
    server.respond("/second", content("arrived"));
    FetchRequest request = requestFor(server.url("/first"), temp.path() / "out");
    request.headers = { "Authorization: Bearer secret" };

    ASSERT_TRUE(fetchToFile(request).has_value());

    const std::vector<std::string> heads = server.requestHeads();
    ASSERT_EQ(heads.size(), 2U);
    EXPECT_NE(heads[0].find("Authorization"), std::string::npos) << heads[0];
    EXPECT_EQ(heads[1].find("Authorization"), std::string::npos) << heads[1];
    EXPECT_EQ(readWholeFile(temp.path() / "out"), "arrived");
}

TEST(UrlFetchTest, ReportsWhatAnAnswerThatNeedsAuthenticationAsksFor)
{
    const TempDirectory temp;
    const std::string asked = R"(Bearer realm="http://127.0.0.1/token",service="registry")";
    const HttpTestServer server{ {
        { "/locked", { .status = 401, .headers = "WWW-Authenticate: " + asked + "\r\n", .body = {}, .stalls = false } },
        { "/missing", { .status = 404, .headers = "WWW-Authenticate: " + asked + "\r\n", .body = {}, .stalls = false } },
    } };

    const auto locked = fetchToFile(requestFor(server.url("/locked"), temp.path() / "out"));
    const auto missing = fetchToFile(requestFor(server.url("/missing"), temp.path() / "out"));

    ASSERT_FALSE(locked.has_value());
    const auto* status = std::get_if<UnexpectedStatus>(&locked.error());
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status, 401);
    EXPECT_EQ(status->authenticate, asked);
    ASSERT_FALSE(missing.has_value());
    ASSERT_NE(std::get_if<UnexpectedStatus>(&missing.error()), nullptr);
    EXPECT_EQ(std::get<UnexpectedStatus>(missing.error()).authenticate, "");
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "out"));
}

TEST(UrlFetchTest, RefusesARedirectToHttpOnAnotherHost)
{
    const TempDirectory temp;
    const HttpTestServer server{ { { "/away", redirectTo("http://example.com/file") } } };
    ASSERT_FALSE(writeNewFile(temp.path() / "out", "previous").operator bool());

    const auto fetched = fetchToFile(requestFor(server.url("/away"), temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    const auto* refused = std::get_if<UrlRefused>(&fetched.error());
    ASSERT_NE(refused, nullptr);
    EXPECT_EQ(refused->url, "http://example.com/file");
    EXPECT_EQ(readWholeFile(temp.path() / "out"), "previous");
}

TEST(UrlFetchTest, StopsARedirectLoop)
{
    const TempDirectory temp;
    const HttpTestServer server{ { { "/loop", redirectTo("/loop") } } };

    const auto fetched = fetchToFile(requestFor(server.url("/loop"), temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    EXPECT_TRUE(std::holds_alternative<TooManyRedirects>(fetched.error()));
    EXPECT_EQ(server.requestLines().size(), 11U);
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "out"));
}

TEST(UrlFetchTest, FollowsAsManyRedirectsAsTheLimit)
{
    const TempDirectory temp;
    std::map<std::string, HttpTestResponse> responses;
    for (int hop = 0; hop < 10; ++hop) {
        responses.emplace("/" + std::to_string(hop), redirectTo("/" + std::to_string(hop + 1)));
    }
    responses.emplace("/10", content("arrived"));
    const HttpTestServer server{ std::move(responses) };

    ASSERT_TRUE(fetchToFile(requestFor(server.url("/0"), temp.path() / "out")).has_value());

    EXPECT_EQ(readWholeFile(temp.path() / "out"), "arrived");
}

TEST(UrlFetchTest, GivesUpOnAServerThatStalls)
{
    const TempDirectory temp;
    const HttpTestServer server{ { { "/stall", { .status = 200, .headers = {}, .body = {}, .stalls = true } } } };
    FetchRequest request = requestFor(server.url("/stall"), temp.path() / "out");
    request.stallLimit = std::chrono::seconds{ 1 };

    const auto fetched = fetchToFile(request);

    ASSERT_FALSE(fetched.has_value());
    EXPECT_TRUE(std::holds_alternative<TransferFailure>(fetched.error()));
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "out"));
}

TEST(UrlFetchTest, ReportsAResponseCutShortAndLeavesTheDestinationAsItWas)
{
    const TempDirectory temp;
    HttpTestResponse response;
    response.body = "part of";
    response.bytesCutOff = 10;
    const HttpTestServer server{ { { "/file", response } } };
    ASSERT_FALSE(writeNewFile(temp.path() / "out", "previous").operator bool());

    const auto fetched = fetchToFile(requestFor(server.url("/file"), temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    EXPECT_TRUE(std::holds_alternative<TransferFailure>(fetched.error()));
    EXPECT_EQ(readWholeFile(temp.path() / "out"), "previous");
    EXPECT_EQ(temp.entries(), (std::vector<std::string>{ "out" }));
}

TEST(UrlFetchTest, ReportsAConnectionThatCannotBeMade)
{
    const TempDirectory temp;
    const auto fetched = fetchToFile(requestFor("http://127.0.0.1:1/file", temp.path() / "out"));

    ASSERT_FALSE(fetched.has_value());
    const auto* failure = std::get_if<TransferFailure>(&fetched.error());
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->url, "http://127.0.0.1:1/file");
    EXPECT_FALSE(failure->message.empty());
}

TEST(UrlFetchTest, ReportsADestinationThatCannotBeWritten)
{
    const TempDirectory temp;
    const HttpTestServer server{ { { "/file", content("body") } } };

    const auto fetched = fetchToFile(requestFor(server.url("/file"), temp.path() / "missing" / "out"));

    ASSERT_FALSE(fetched.has_value());
    const auto* failure = std::get_if<CannotWriteDestination>(&fetched.error());
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->code, std::errc::no_such_file_or_directory);
    EXPECT_EQ(failure->path, temp.path() / "missing" / "out");
    EXPECT_TRUE(server.requestLines().empty());
}

TEST(UrlFetchTest, ReportsADestinationThatCannotBeReplaced)
{
    const TempDirectory temp;
    const HttpTestServer server{ { { "/file", content("body") } } };
    std::filesystem::create_directory(temp.path() / "directory");
    ASSERT_FALSE(writeNewFile(temp.path() / "directory" / "held", "").operator bool());

    const auto fetched = fetchToFile(requestFor(server.url("/file"), temp.path() / "directory"));

    ASSERT_FALSE(fetched.has_value());
    EXPECT_TRUE(std::holds_alternative<CannotWriteDestination>(fetched.error()));
    EXPECT_TRUE(std::filesystem::is_directory(temp.path() / "directory"));
}

TEST(UrlFetchTest, ReachesThisMachineDirectlyWhenAProxyIsSet)
{
    const TempDirectory temp;
    const HttpTestServer proxy{ {} };
    const HttpTestServer server{ { { "/file", content("direct") } } };
    const ScopedVariable variable{ "http_proxy", proxy.url("") };

    ASSERT_TRUE(fetchToFile(requestFor(server.url("/file"), temp.path() / "out")).has_value());

    EXPECT_EQ(readWholeFile(temp.path() / "out"), "direct");
    EXPECT_TRUE(proxy.requestLines().empty());
}

/**
 * The name need not resolve: asking the proxy to connect is what shows it was
 * used. The certificates are not reached, since the proxy refuses.
 */
TEST(UrlFetchTest, GoesThroughAProxyTheEnvironmentNames)
{
    const TempDirectory temp;
    const HttpTestServer proxy{ {} };
    const ScopedVariable variable{ "https_proxy", proxy.url("") };
    const ScopedVariable noException{ "no_proxy", "" };
    FetchRequest request = requestFor("https://scrap-test.invalid/file", temp.path() / "out");
    request.certificateBundle = temp.path() / "unused.pem";

    const auto fetched = fetchToFile(request);

    ASSERT_FALSE(fetched.has_value());
    EXPECT_EQ(proxy.requestLines(), (std::vector<std::string>{ "CONNECT scrap-test.invalid:443 HTTP/1.1" }));
}

TEST(UrlFetchTest, TakesContentOfExactlyTheSizeLimit)
{
    const TempDirectory directory;
    const HttpTestServer server({ { "/file", content("12345") } });
    const std::filesystem::path destination = directory.path() / "file";

    FetchRequest request = requestFor(server.url("/file"), destination);
    request.sizeLimit = 5;

    ASSERT_TRUE(fetchToFile(request).has_value());
    EXPECT_EQ(readWholeFile(destination).value_or(""), "12345");
}

TEST(UrlFetchTest, RefusesContentPastTheSizeLimitAndLeavesTheDestinationAlone)
{
    const TempDirectory directory;
    const HttpTestServer server({ { "/file", content("123456") } });
    const std::filesystem::path destination = directory.writeFile("file", "before");

    FetchRequest request = requestFor(server.url("/file"), destination);
    request.sizeLimit = 5;

    const auto fetched = fetchToFile(request);

    ASSERT_FALSE(fetched.has_value());
    const auto* tooLarge = std::get_if<ContentTooLarge>(&fetched.error());
    ASSERT_NE(tooLarge, nullptr);
    EXPECT_EQ(tooLarge->limit, 5U);
    EXPECT_EQ(readWholeFile(destination).value_or(""), "before");
}

TEST(UrlFetchTest, CountsOnlyTheContentAgainstTheSizeLimit)
{
    const TempDirectory directory;
    const HttpTestServer server({ { "/from", redirectTo("/to") }, { "/to", content("12345") } });
    const std::filesystem::path destination = directory.path() / "file";

    FetchRequest request = requestFor(server.url("/from"), destination);
    request.sizeLimit = 5;

    ASSERT_TRUE(fetchToFile(request).has_value());
    EXPECT_EQ(readWholeFile(destination).value_or(""), "12345");
}
