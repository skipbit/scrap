#include "registry/IngotManifest.h"

#include "json/JsonParser.h"
#include "json/JsonValue.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Registry {

namespace {

constexpr std::string_view ManifestMediaType = "application/vnd.oci.image.manifest.v1+json";
constexpr std::string_view IngotLayerMediaType = "application/x-tar+xz";
constexpr std::string_view DigestPrefix = "sha256:";
constexpr std::size_t Sha256HexLength = 64;

bool isLowercaseHex(const char c)
{
    return ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f'));
}

bool isSha256Digest(const std::string_view digest)
{
    return digest.starts_with(DigestPrefix) && (digest.size() == DigestPrefix.size() + Sha256HexLength) && std::ranges::all_of(digest.substr(DigestPrefix.size()), isLowercaseHex);
}

bool hasString(const Json::Value& value, const std::string_view name, const std::string_view expected)
{
    const Json::Value* member = value.member(name);
    return (member != nullptr) && (member->asString() != nullptr) && (*member->asString() == expected);
}

std::expected<IngotLayer, std::string> readLayer(const Json::Value& layer)
{
    if (! hasString(layer, "mediaType", IngotLayerMediaType)) {
        return std::unexpected("its layer is not " + std::string{ IngotLayerMediaType });
    }
    const Json::Value* digest = layer.member("digest");
    if ((digest == nullptr) || (digest->asString() == nullptr) || (! isSha256Digest(*digest->asString()))) {
        return std::unexpected(std::string{ "its layer has no SHA-256 digest" });
    }
    const Json::Value* size = layer.member("size");
    const std::optional<std::uint64_t> bytes = (size == nullptr) ? std::nullopt : size->asUnsigned();
    if (! bytes.has_value()) {
        return std::unexpected(std::string{ "its layer has no size" });
    }
    return IngotLayer{ .digest = *digest->asString(), .size = *bytes };
}

}  // anonymous namespace

std::expected<IngotLayer, std::string> readIngotManifest(const std::string_view text)
{
    const auto manifest = Json::parse(text);
    if (! manifest.has_value()) {
        return std::unexpected("it is not JSON: " + manifest.error().reason + " at byte " + std::to_string(manifest.error().offset));
    }
    if (! hasString(*manifest, "mediaType", ManifestMediaType)) {
        return std::unexpected("it is not " + std::string{ ManifestMediaType });
    }
    const Json::Value* layers = manifest->member("layers");
    if ((layers == nullptr) || (layers->asArray() == nullptr) || (layers->asArray()->size() != 1)) {
        return std::unexpected(std::string{ "it does not hold exactly one layer" });
    }
    return readLayer(layers->asArray()->front());
}

}  // namespace scrap::Registry
