#pragma once

#include "fetch/UrlFetch.h"
#include "registry/IngotLocation.h"

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <variant>

namespace scrap::Registry {

/**
 * @brief What the registry gave at the URL is not what was asked for: the
 *        manifest is not the one the index names, or the layer not the one the
 *        manifest names.
 */
struct ContentDoesNotMatch {
    std::string url;
};

/**
 * @brief The registry answered at the URL with something that cannot be used.
 */
struct AnswerUnusable {
    std::string url;
    std::string reason;
};

/**
 * @brief The fetched layer could not be read back to check its digest.
 */
struct FetchedFileUnreadable {
    std::filesystem::path path;
    std::error_code code;
};

using RegistryFailure = std::variant<Fetch::FetchError, ContentDoesNotMatch, AnswerUnusable, FetchedFileUnreadable>;

/**
 * @brief Which ingot to fetch, and where to put it.
 */
struct RegistryFetchRequest {
    IngotLocation location;
    std::filesystem::path destination;
    std::optional<std::filesystem::path> certificateBundle;
    /// How many bytes the ingot may hold.
    std::uint64_t sizeLimit = 0;
};

/**
 * @brief Fetch an ingot from an OCI registry into the destination.
 *
 * The manifest is asked for by its digest and must have it; when the registry
 * asks for a bearer token first, one is fetched anonymously from where its
 * challenge says. The layer the manifest names is then fetched, and must have
 * the digest the manifest gives and be no larger than it says. The token goes to the registry alone,
 * never to a host a redirect leads to.
 *
 * @return Nothing once the destination holds the ingot, or what went wrong.
 */
[[nodiscard]] std::expected<void, RegistryFailure> fetchIngot(const RegistryFetchRequest& request);

}  // namespace scrap::Registry
