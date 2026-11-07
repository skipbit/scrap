#pragma once

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <string>
#include <string_view>

namespace scrap::Registry {

/**
 * @brief The one layer of an ingot's manifest: the ingot itself.
 */
struct IngotLayer {
    std::string digest;  ///< sha256: and 64 lowercase hex digits.
    std::uint64_t size = 0;
};

/**
 * @brief Read the OCI image manifest of an ingot.
 *
 * The manifest must say it is an OCI image manifest and hold exactly one layer,
 * an xz-compressed tar archive with a SHA-256 digest and a size.
 *
 * @return The layer, or why the manifest is not one of an ingot.
 */
[[nodiscard]] std::expected<IngotLayer, std::string> readIngotManifest(std::string_view text);

}  // namespace scrap::Registry
