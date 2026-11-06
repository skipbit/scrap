#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace scrap::Registry {

/**
 * @brief Where an ingot is kept in an OCI registry.
 */
struct IngotLocation {
    std::string origin;      ///< e.g. https://ghcr.io
    std::string repository;  ///< e.g. skipbit/scrap-toolchain/gcc
    std::string digest;      ///< The manifest's, e.g. sha256:70f6...

    [[nodiscard]] std::string manifestUrl() const;
    [[nodiscard]] std::string blobUrl(std::string_view blobDigest) const;

    /// What a token must allow to fetch from the repository.
    [[nodiscard]] std::string pullScope() const;
};

/**
 * @brief Where the ingot an index names by @p registry, @p tag and @p digest
 *        is kept.
 *
 * The registry is a host, with a port if need be, and a path below it, such as
 * ghcr.io/skipbit/scrap-toolchain. The name before the colon of the tag, such
 * as gcc in gcc:15.2.0, completes the repository. The registry is reached over
 * https, or over http when it is this machine.
 *
 * @return The location, or nothing when the registry names no host or the tag
 *         no name.
 */
[[nodiscard]] std::optional<IngotLocation> locateIngot(std::string_view registry, std::string_view tag, std::string_view digest);

}  // namespace scrap::Registry
