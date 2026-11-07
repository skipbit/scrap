#include "registry/RegistryFetch.h"

#include "digest/DigestText.h"
#include "digest/Sha256.h"
#include "digest/Sha256File.h"
#include "fetch/UrlFetch.h"
#include "registry/BearerChallenge.h"
#include "registry/IngotLocation.h"
#include "registry/IngotManifest.h"
#include "registry/TokenRequest.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Registry {

namespace {

constexpr std::uint64_t TokenSizeLimit = 1024ULL * 1024;
constexpr std::uint64_t ManifestSizeLimit = 4ULL * 1024 * 1024;
constexpr long HttpUnauthorized = 401;
constexpr std::string_view AcceptManifest = "Accept: application/vnd.oci.image.manifest.v1+json";

std::expected<std::string, RegistryFailure> fetchText(const std::string& url, const RegistryFetchRequest& request, std::vector<std::string> headers, const std::uint64_t sizeLimit)
{
    auto fetched = Fetch::fetchToString({ .url = url, .certificateBundle = request.certificateBundle, .sizeLimit = sizeLimit, .headers = std::move(headers) });
    if (! fetched.has_value()) {
        return std::unexpected(std::move(fetched.error()));
    }
    return std::move(*fetched);
}

std::expected<void, RegistryFailure> fetchInto(const std::string& url, const std::filesystem::path& destination, const RegistryFetchRequest& request, std::vector<std::string> headers, const std::uint64_t sizeLimit)
{
    auto fetched = Fetch::fetchToFile({ .url = url, .destination = destination, .certificateBundle = request.certificateBundle, .sizeLimit = sizeLimit, .headers = std::move(headers) });
    if (! fetched.has_value()) {
        return std::unexpected(std::move(fetched.error()));
    }
    return {};
}

/**
 * The header lines that carry @p token, when there is one.
 */
std::vector<std::string> authorization(const std::optional<std::string>& token)
{
    if (! token.has_value()) {
        return {};
    }
    return { "Authorization: Bearer " + *token };
}

std::vector<std::string> manifestHeaders(const std::optional<std::string>& token)
{
    std::vector<std::string> headers = authorization(token);
    headers.insert(headers.begin(), std::string{ AcceptManifest });
    return headers;
}

/**
 * The token a registry hands out at the realm of @p challenge.
 */
std::expected<std::string, RegistryFailure> fetchToken(const BearerChallenge& challenge, const RegistryFetchRequest& request)
{
    const std::string url = tokenUrl(challenge, request.location.pullScope());
    auto text = fetchText(url, request, {}, TokenSizeLimit);
    if (! text.has_value()) {
        return std::unexpected(std::move(text.error()));
    }
    auto token = readTokenAnswer(*text);
    if (! token.has_value()) {
        return std::unexpected(AnswerUnusable{ .url = url, .reason = "it gave no token" });
    }
    return std::move(*token);
}

/**
 * The manifest, and the token it took to get it, when one was needed.
 */
struct FetchedManifest {
    std::string text;
    std::optional<std::string> token;
};

/**
 * Fetch the manifest, with a token when the registry asks for one.
 */
std::expected<FetchedManifest, RegistryFailure> fetchManifest(const RegistryFetchRequest& request)
{
    const std::string url = request.location.manifestUrl();
    auto fetched = fetchText(url, request, manifestHeaders(std::nullopt), ManifestSizeLimit);
    if (fetched.has_value()) {
        return FetchedManifest{ .text = std::move(*fetched), .token = std::nullopt };
    }

    const auto* failure = std::get_if<Fetch::FetchError>(&fetched.error());
    const auto* status = (failure == nullptr) ? nullptr : std::get_if<Fetch::UnexpectedStatus>(failure);
    if ((status == nullptr) || (status->status != HttpUnauthorized)) {
        return std::unexpected(std::move(fetched.error()));
    }
    const auto challenge = readBearerChallenge(status->authenticate);
    if (! challenge.has_value()) {
        return std::unexpected(AnswerUnusable{ .url = url, .reason = "it asks for authentication other than an anonymous token" });
    }
    auto token = fetchToken(*challenge, request);
    if (! token.has_value()) {
        return std::unexpected(std::move(token.error()));
    }

    auto again = fetchText(url, request, manifestHeaders(*token), ManifestSizeLimit);
    if (! again.has_value()) {
        return std::unexpected(std::move(again.error()));
    }
    return FetchedManifest{ .text = std::move(*again), .token = std::move(*token) };
}

std::expected<void, RegistryFailure> checkDigest(const std::filesystem::path& file, const std::string_view digest, const std::string& url)
{
    const auto actual = Digest::sha256OfFile(file);
    if (! actual.has_value()) {
        return std::unexpected(FetchedFileUnreadable{ .path = file, .code = actual.error() });
    }
    if (Digest::sha256Digest(*actual) != digest) {
        return std::unexpected(ContentDoesNotMatch{ .url = url });
    }
    return {};
}

std::expected<void, RegistryFailure> fetchLayer(const RegistryFetchRequest& request, const IngotLayer& layer, const std::optional<std::string>& token)
{
    const std::string url = request.location.blobUrl(layer.digest);
    if (layer.size > request.sizeLimit) {
        return std::unexpected(Fetch::FetchError{ Fetch::ContentTooLarge{ .url = url, .limit = request.sizeLimit } });
    }
    if (auto fetched = fetchInto(url, request.destination, request, authorization(token), layer.size); ! fetched.has_value()) {
        // The limit was the size the manifest gives, so more is not that layer.
        const auto* failure = std::get_if<Fetch::FetchError>(&fetched.error());
        if ((failure != nullptr) && std::holds_alternative<Fetch::ContentTooLarge>(*failure)) {
            return std::unexpected(ContentDoesNotMatch{ .url = url });
        }
        return std::unexpected(std::move(fetched.error()));
    }
    return checkDigest(request.destination, layer.digest, url);
}

}  // anonymous namespace

std::expected<void, RegistryFailure> fetchIngot(const RegistryFetchRequest& request)
{
    auto manifest = fetchManifest(request);
    if (! manifest.has_value()) {
        return std::unexpected(std::move(manifest.error()));
    }
    const std::string url = request.location.manifestUrl();
    Digest::Sha256 sha256;
    sha256.update(manifest->text);
    if (Digest::sha256Digest(sha256.hexDigest()) != request.location.digest) {
        return std::unexpected(ContentDoesNotMatch{ .url = url });
    }
    auto layer = readIngotManifest(manifest->text);
    if (! layer.has_value()) {
        return std::unexpected(AnswerUnusable{ .url = url, .reason = "its manifest is not one of an ingot: " + layer.error() });
    }
    return fetchLayer(request, *layer, manifest->token);
}

}  // namespace scrap::Registry
