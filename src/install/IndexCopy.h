#pragma once

#include "fetch/UrlFetch.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <variant>

namespace scrap::Install {

/**
 * @brief Where the index comes from.
 */
struct IndexSource {
    std::string url;
    std::optional<std::filesystem::path> certificateBundle;
};

/**
 * @brief The index to choose from.
 */
struct ObtainedIndex {
    Toolchain::ToolchainIndex index;
    std::optional<std::string> copyFetchedAt;  ///< When the saved copy in use was fetched, in UTC; nothing when the index is fresh.
};

/**
 * @brief No index could be fetched, and no saved copy can stand in.
 */
struct IndexUnavailable {
    std::string reason;
    std::optional<Fetch::FetchError> fetchError;  ///< The failure of the transfer, when it was one.
};

/**
 * @brief The index declares a format this scrap does not read.
 */
struct IndexFormatUnsupported {
    std::string format;
};

using IndexFault = std::variant<IndexUnavailable, IndexFormatUnsupported>;

/**
 * @brief Fetch the index and save it below @p scrapHome, or fall back on the
 *        copy saved before.
 *
 * What was fetched replaces the saved copy only once it has been read in full,
 * so a transfer cut short or an index its maker got wrong leaves the previous
 * copy as it was. An index in a format this scrap does not read is not passed
 * over, since an older copy would hide that scrap needs updating.
 *
 * @return The index, or why there is none.
 */
[[nodiscard]] std::expected<ObtainedIndex, IndexFault> obtainIndex(const std::filesystem::path& scrapHome, const IndexSource& source, const Toolchain::ToolchainVersion& scrapVersion);

}  // namespace scrap::Install
