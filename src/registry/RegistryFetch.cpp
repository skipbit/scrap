#include "registry/RegistryFetch.h"

#include "digest/Sha256File.h"
#include "fetch/UrlFetch.h"
#include "file/WholeFile.h"
#include "json/JsonParser.h"
#include "json/JsonValue.h"
#include "registry/BearerChallenge.h"
#include "registry/IngotLocation.h"
#include "registry/IngotManifest.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Registry {

namespace {

constexpr std::uint64_t TokenSizeLimit = 1024ULL * 1024;
constexpr std::uint64_t ManifestSizeLimit = 4ULL * 1024 * 1024;
constexpr long HttpUnauthorized = 401;
constexpr std::string_view AcceptManifest = "Accept: application/vnd.oci.image.manifest.v1+json";
constexpr std::string_view DigestPrefix = "sha256:";
constexpr std::array<char, 16> HexDigits{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
constexpr unsigned HighNibbleShift = 4;
constexpr unsigned NibbleMask = 0xF;

/**
 * @p text with everything but letters, digits and -._~ percent-encoded, for a
 * query parameter.
 */
std::string percentEncoded(const std::string_view text)
{
    std::string encoded;
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if ((std::isalnum(byte) != 0) || (c == '-') || (c == '.') || (c == '_') || (c == '~')) {
            encoded.push_back(c);
        } else {
            encoded.push_back('%');
            encoded.push_back(HexDigits.at(byte >> HighNibbleShift));
            encoded.push_back(HexDigits.at(byte & NibbleMask));
        }
    }
    return encoded;
}

std::string tokenUrl(const BearerChallenge& challenge, const std::string& defaultScope)
{
    std::string url = challenge.realm;
    url += url.contains('?') ? '&' : '?';
    if (! challenge.service.empty()) {
        url += "service=" + percentEncoded(challenge.service) + "&";
    }
    url += "scope=" + percentEncoded(challenge.scope.empty() ? defaultScope : challenge.scope);
    return url;
}

std::expected<std::string, RegistryFailure> readFetched(const std::filesystem::path& file)
{
    auto text = File::readWholeFile(file);
    if (! text.has_value()) {
        // Reading a whole file reports the step that failed rather than an error code.
        return std::unexpected(FetchedFileUnreadable{ .path = file, .code = std::make_error_code(std::errc::io_error) });
    }
    return std::move(*text);
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
 * The token a registry hands out at the realm of @p challenge.
 */
std::expected<std::string, RegistryFailure> fetchToken(const BearerChallenge& challenge, const RegistryFetchRequest& request)
{
    const std::string url = tokenUrl(challenge, request.location.pullScope());
    const std::filesystem::path file = request.workDirectory / "token.json";
    if (auto fetched = fetchInto(url, file, request, {}, TokenSizeLimit); ! fetched.has_value()) {
        return std::unexpected(std::move(fetched.error()));
    }
    auto text = readFetched(file);
    if (! text.has_value()) {
        return std::unexpected(std::move(text.error()));
    }
    const auto answer = Json::parse(*text);
    if (answer.has_value()) {
        for (const std::string_view name : { "token", "access_token" }) {
            const std::string* token = answer->stringMember(name);
            if ((token != nullptr) && (! token->empty())) {
                return *token;
            }
        }
    }
    return std::unexpected(AnswerUnusable{ .url = url, .reason = "it gave no token" });
}

/**
 * Fetch the manifest into @p file, with a token when the registry asks for one.
 *
 * @return The header lines that authorise further requests, or what went wrong.
 */
std::expected<std::vector<std::string>, RegistryFailure> fetchManifest(const RegistryFetchRequest& request, const std::filesystem::path& file)
{
    const std::string url = request.location.manifestUrl();
    std::vector<std::string> headers{ std::string{ AcceptManifest } };
    auto fetched = fetchInto(url, file, request, headers, ManifestSizeLimit);
    if (fetched.has_value()) {
        return std::vector<std::string>{};
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

    std::vector<std::string> authorization{ "Authorization: Bearer " + *token };
    headers.push_back(authorization.front());
    if (auto again = fetchInto(url, file, request, headers, ManifestSizeLimit); ! again.has_value()) {
        return std::unexpected(std::move(again.error()));
    }
    return authorization;
}

std::expected<void, RegistryFailure> checkDigest(const std::filesystem::path& file, const std::string_view digest, const std::string& url)
{
    const auto actual = Digest::sha256OfFile(file);
    if (! actual.has_value()) {
        return std::unexpected(FetchedFileUnreadable{ .path = file, .code = actual.error() });
    }
    if (std::string{ DigestPrefix } + *actual != digest) {
        return std::unexpected(ContentDoesNotMatch{ .url = url });
    }
    return {};
}

std::expected<void, RegistryFailure> fetchLayer(const RegistryFetchRequest& request, const IngotLayer& layer, std::vector<std::string> authorization)
{
    const std::string url = request.location.blobUrl(layer.digest);
    if (layer.size > request.sizeLimit) {
        return std::unexpected(Fetch::FetchError{ Fetch::ContentTooLarge{ .url = url, .limit = request.sizeLimit } });
    }
    if (auto fetched = fetchInto(url, request.destination, request, std::move(authorization), layer.size); ! fetched.has_value()) {
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
    const std::filesystem::path manifestFile = request.workDirectory / "manifest.json";
    auto authorization = fetchManifest(request, manifestFile);
    if (! authorization.has_value()) {
        return std::unexpected(std::move(authorization.error()));
    }
    const std::string url = request.location.manifestUrl();
    if (auto checked = checkDigest(manifestFile, request.location.digest, url); ! checked.has_value()) {
        return checked;
    }
    auto text = readFetched(manifestFile);
    if (! text.has_value()) {
        return std::unexpected(std::move(text.error()));
    }
    auto layer = readIngotManifest(*text);
    if (! layer.has_value()) {
        return std::unexpected(AnswerUnusable{ .url = url, .reason = "its manifest is not one of an ingot: " + layer.error() });
    }
    return fetchLayer(request, *layer, std::move(*authorization));
}

}  // namespace scrap::Registry
