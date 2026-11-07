#include "registry/RegistryFetch.h"

#include "digest/Sha256.h"
#include "fetch/UrlFetch.h"
#include "file/WholeFile.h"
#include "registry/IngotLocation.h"
#include "support/HttpTestServer.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using namespace scrap::Registry;
using scrap::Digest::Sha256;
using scrap::File::readWholeFile;
using scrap::TestSupport::HttpTestResponse;
using scrap::TestSupport::HttpTestServer;
using scrap::TestSupport::TempDirectory;

namespace {

constexpr std::string_view Token = "anonymous-token";
constexpr std::uint64_t Limit = 1024;

std::string digestOf(const std::string& bytes)
{
    Sha256 digest;
    digest.update(bytes);
    return "sha256:" + digest.hexDigest();
}

std::string manifestFor(const std::string& layerDigest, const std::uint64_t size)
{
    return R"({"schemaVersion": 2, "mediaType": "application/vnd.oci.image.manifest.v1+json", "layers": [{"mediaType": "application/x-tar+xz", "digest": ")"
           + layerDigest + R"(", "size": )" + std::to_string(size) + "}]}";
}

HttpTestResponse ok(std::string body)
{
    return { .status = 200, .headers = {}, .body = std::move(body), .stalls = false };
}

/**
 * A registry on this machine that keeps one ingot in skipbit/toolchains/gcc
 * and asks for a token, as ghcr.io does. Its blob redirects to storage on
 * another host name for this machine, which must not see the token.
 */
class FakeRegistry {
public:
    explicit FakeRegistry(std::string ingot = "the bytes of an ingot")
        : _ingot(std::move(ingot))
    {
        serve(manifestFor(digestOf(_ingot), _ingot.size()));
        _server.respond(tokenPath(), ok(R"({"token": ")" + std::string{ Token } + R"("})"));
        _server.respond(blobPath(), locked({ .status = 307, .headers = "Location: http://localhost:" + std::to_string(_server.port()) + "/storage\r\n", .body = {}, .stalls = false }));
        _server.respond("/storage", ok(_ingot));
    }

    /// Serve @p manifest, under the digest the index gives.
    void serve(const std::string& manifest)
    {
        _manifest = manifest;
        _server.respond(manifestPath(), locked(ok(manifest)));
    }

    [[nodiscard]] IngotLocation location() const
    {
        return { .origin = "http://127.0.0.1:" + std::to_string(_server.port()), .repository = "skipbit/toolchains/gcc", .digest = digestOf(_manifest) };
    }

    [[nodiscard]] std::string manifestPath() const
    {
        return "/v2/skipbit/toolchains/gcc/manifests/" + digestOf(_manifest);
    }

    [[nodiscard]] std::string blobPath() const
    {
        return "/v2/skipbit/toolchains/gcc/blobs/" + digestOf(_ingot);
    }

    [[nodiscard]] static std::string tokenPath()
    {
        return "/token?service=registry&scope=repository%3Askipbit%2Ftoolchains%2Fgcc%3Apull";
    }

    [[nodiscard]] std::string challenge() const
    {
        return R"(Bearer realm="http://127.0.0.1:)" + std::to_string(_server.port()) + R"(/token",service="registry")";
    }

    HttpTestResponse locked(HttpTestResponse response) const
    {
        response.requiredHeader = "Authorization: Bearer " + std::string{ Token };
        response.challenge = challenge();
        return response;
    }

    HttpTestServer& server()
    {
        return _server;
    }

private:
    std::string _ingot;
    std::string _manifest;
    HttpTestServer _server{ {} };
};

RegistryFetchRequest requestFor(const FakeRegistry& registry, const TempDirectory& temp, const std::uint64_t limit = Limit)
{
    return { .location = registry.location(), .destination = temp.path() / "ingot", .certificateBundle = std::nullopt, .sizeLimit = limit };
}

}  // anonymous namespace

TEST(RegistryFetchTest, FetchesAnIngotWithAnAnonymousToken)
{
    const TempDirectory temp;
    FakeRegistry registry;

    const auto fetched = fetchIngot(requestFor(registry, temp));

    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(readWholeFile(temp.path() / "ingot"), "the bytes of an ingot");
    const std::vector<std::string> heads = registry.server().requestHeads();
    ASSERT_EQ(heads.size(), 5U);
    EXPECT_TRUE(heads[0].starts_with("GET " + registry.manifestPath() + " "));
    EXPECT_NE(heads[0].find("\r\nAccept: application/vnd.oci.image.manifest.v1+json\r\n"), std::string::npos) << heads[0];
    EXPECT_TRUE(heads[1].starts_with("GET " + FakeRegistry::tokenPath() + " "));
    EXPECT_EQ(heads[1].find("Authorization"), std::string::npos) << heads[1];
    EXPECT_TRUE(heads[2].starts_with("GET " + registry.manifestPath() + " "));
    EXPECT_TRUE(heads[3].starts_with("GET " + registry.blobPath() + " "));
    EXPECT_TRUE(heads[4].starts_with("GET /storage "));
    EXPECT_EQ(heads[4].find("Authorization"), std::string::npos) << heads[4];
}

TEST(RegistryFetchTest, FetchesFromARegistryThatAsksForNoToken)
{
    const TempDirectory temp;
    FakeRegistry registry;
    registry.server().respond(registry.manifestPath(), ok(manifestFor(digestOf("the bytes of an ingot"), 21)));
    registry.server().respond(registry.blobPath(), ok("the bytes of an ingot"));

    const auto fetched = fetchIngot(requestFor(registry, temp));

    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(readWholeFile(temp.path() / "ingot"), "the bytes of an ingot");
    for (const std::string& head : registry.server().requestHeads()) {
        EXPECT_EQ(head.find("Authorization"), std::string::npos) << head;
    }
}

TEST(RegistryFetchTest, RefusesAManifestOtherThanTheIndexNames)
{
    const TempDirectory temp;
    FakeRegistry registry;
    RegistryFetchRequest request = requestFor(registry, temp);
    registry.server().respond(registry.manifestPath(), registry.locked(ok(manifestFor(digestOf("the bytes of an ingot"), 21) + " ")));

    const auto fetched = fetchIngot(request);

    ASSERT_FALSE(fetched.has_value());
    const auto* mismatch = std::get_if<ContentDoesNotMatch>(&fetched.error());
    ASSERT_NE(mismatch, nullptr);
    EXPECT_EQ(mismatch->url, request.location.manifestUrl());
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "ingot"));
}

TEST(RegistryFetchTest, RefusesALayerOtherThanTheManifestNames)
{
    for (const std::string& stored : { std::string{ "the bytes of an ingoT" }, std::string{ "the bytes of an ingot, and more" }, std::string{ "short" } }) {
        const TempDirectory temp;
        FakeRegistry registry;
        registry.server().respond("/storage", ok(stored));

        const auto fetched = fetchIngot(requestFor(registry, temp));

        ASSERT_FALSE(fetched.has_value()) << stored;
        const auto* mismatch = std::get_if<ContentDoesNotMatch>(&fetched.error());
        ASSERT_NE(mismatch, nullptr) << stored;
        EXPECT_EQ(mismatch->url, registry.location().blobUrl(digestOf("the bytes of an ingot"))) << stored;
    }
}

TEST(RegistryFetchTest, RefusesALayerLargerThanTheLimitBeforeFetchingIt)
{
    const TempDirectory temp;
    FakeRegistry registry;

    const auto fetched = fetchIngot(requestFor(registry, temp, 20));

    ASSERT_FALSE(fetched.has_value());
    const auto* failure = std::get_if<scrap::Fetch::FetchError>(&fetched.error());
    ASSERT_NE(failure, nullptr);
    const auto* tooLarge = std::get_if<scrap::Fetch::ContentTooLarge>(failure);
    ASSERT_NE(tooLarge, nullptr);
    EXPECT_EQ(tooLarge->limit, 20U);
    for (const std::string& line : registry.server().requestLines()) {
        EXPECT_EQ(line.find("/blobs/"), std::string::npos) << line;
    }
}

TEST(RegistryFetchTest, ReportsAManifestThatIsNotOneOfAnIngot)
{
    const TempDirectory temp;
    FakeRegistry registry;
    registry.serve(R"({"mediaType": "application/vnd.oci.image.manifest.v1+json", "layers": []})");

    const auto fetched = fetchIngot(requestFor(registry, temp));

    ASSERT_FALSE(fetched.has_value());
    const auto* unusable = std::get_if<AnswerUnusable>(&fetched.error());
    ASSERT_NE(unusable, nullptr);
    EXPECT_EQ(unusable->url, registry.location().manifestUrl());
    EXPECT_EQ(unusable->reason, "its manifest is not one of an ingot: it does not hold exactly one layer");
}

TEST(RegistryFetchTest, ReportsARegistryThatGivesNoToken)
{
    for (const std::string& answer : { std::string{ R"({"expires_in": 300})" }, std::string{ R"({"token": ""})" }, std::string{ R"({"token": 1})" }, std::string{ "token" } }) {
        const TempDirectory temp;
        FakeRegistry registry;
        registry.server().respond(FakeRegistry::tokenPath(), ok(answer));

        const auto fetched = fetchIngot(requestFor(registry, temp));

        ASSERT_FALSE(fetched.has_value()) << answer;
        const auto* unusable = std::get_if<AnswerUnusable>(&fetched.error());
        ASSERT_NE(unusable, nullptr) << answer;
        EXPECT_EQ(unusable->url, registry.location().origin + FakeRegistry::tokenPath()) << answer;
        EXPECT_EQ(unusable->reason, "it gave no token") << answer;
    }
}

TEST(RegistryFetchTest, ReportsAManifestTheRegistryDoesNotHave)
{
    const TempDirectory temp;
    FakeRegistry registry;
    RegistryFetchRequest request = requestFor(registry, temp);
    request.location.digest = digestOf("another manifest");

    const auto fetched = fetchIngot(request);

    ASSERT_FALSE(fetched.has_value());
    const auto* failure = std::get_if<scrap::Fetch::FetchError>(&fetched.error());
    ASSERT_NE(failure, nullptr);
    const auto* status = std::get_if<scrap::Fetch::UnexpectedStatus>(failure);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status, 404);
    EXPECT_EQ(status->url, request.location.manifestUrl());
}

TEST(RegistryFetchTest, TakesATokenGivenAsAnAccessToken)
{
    const TempDirectory temp;
    FakeRegistry registry;
    registry.server().respond(FakeRegistry::tokenPath(), ok(R"({"access_token": ")" + std::string{ Token } + R"("})"));

    EXPECT_TRUE(fetchIngot(requestFor(registry, temp)).has_value());
}

TEST(RegistryFetchTest, ReportsAChallengeOtherThanForABearerToken)
{
    const TempDirectory temp;
    FakeRegistry registry;
    HttpTestResponse basic = registry.locked(ok("{}"));
    basic.challenge = R"(Basic realm="registry")";
    registry.server().respond(registry.manifestPath(), basic);

    const auto fetched = fetchIngot(requestFor(registry, temp));

    ASSERT_FALSE(fetched.has_value());
    const auto* unusable = std::get_if<AnswerUnusable>(&fetched.error());
    ASSERT_NE(unusable, nullptr);
    EXPECT_EQ(unusable->reason, "it asks for authentication other than an anonymous token");
}

TEST(RegistryFetchTest, ReportsARegistryThatRefusesItsOwnToken)
{
    const TempDirectory temp;
    FakeRegistry registry;
    registry.server().respond(FakeRegistry::tokenPath(), ok(R"({"token": "another"})"));

    const auto fetched = fetchIngot(requestFor(registry, temp));

    ASSERT_FALSE(fetched.has_value());
    const auto* failure = std::get_if<scrap::Fetch::FetchError>(&fetched.error());
    ASSERT_NE(failure, nullptr);
    const auto* status = std::get_if<scrap::Fetch::UnexpectedStatus>(failure);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status, 401);
}

TEST(RegistryFetchTest, AsksForTheScopeTheChallengeNamesAtARealmWithAQuery)
{
    const TempDirectory temp;
    FakeRegistry registry;
    const std::string realm = "http://127.0.0.1:" + std::to_string(registry.server().port()) + "/auth?client=scrap";
    HttpTestResponse manifest = registry.locked(ok(manifestFor(digestOf("the bytes of an ingot"), 21)));
    manifest.challenge = R"(Bearer realm=")" + realm + R"(",scope="repository:skipbit/toolchains/gcc:pull,push")";
    registry.server().respond(registry.manifestPath(), manifest);
    registry.server().respond("/auth?client=scrap&scope=repository%3Askipbit%2Ftoolchains%2Fgcc%3Apull%2Cpush", ok(R"({"token": ")" + std::string{ Token } + R"("})"));

    EXPECT_TRUE(fetchIngot(requestFor(registry, temp)).has_value());
}
