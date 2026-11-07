#include "registry/IngotManifest.h"

#include "digest/DigestText.h"
#include "json/JsonParser.h"
#include "json/JsonValue.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Registry {

namespace {

constexpr std::string_view ManifestMediaType = "application/vnd.oci.image.manifest.v1+json";
constexpr std::string_view IngotLayerMediaType = "application/x-tar+xz";
bool hasString(const Json::Value& value, const std::string_view name, const std::string_view expected)
{
    const std::string* actual = value.stringMember(name);
    return (actual != nullptr) && (*actual == expected);
}

std::expected<IngotLayer, std::string> readLayer(const Json::Value& layer)
{
    if (! hasString(layer, "mediaType", IngotLayerMediaType)) {
        return std::unexpected("its layer is not " + std::string{ IngotLayerMediaType });
    }
    const std::string* digest = layer.stringMember("digest");
    if ((digest == nullptr) || (! Digest::isSha256Digest(*digest))) {
        return std::unexpected(std::string{ "its layer has no SHA-256 digest" });
    }
    const Json::Value* size = layer.member("size");
    const std::optional<std::uint64_t> bytes = (size == nullptr) ? std::nullopt : size->asUnsigned();
    if (! bytes.has_value()) {
        return std::unexpected(std::string{ "its layer has no size" });
    }
    return IngotLayer{ .digest = *digest, .size = *bytes };
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
