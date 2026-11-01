#pragma once

#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace scrap::Toolchain {

/**
 * @brief What kind of failure a ToolchainIndexError reports.
 */
enum class ToolchainIndexErrorKind : std::uint8_t {
    Malformed,         ///< The text breaks TOML or the rules of the index.
    UnsupportedFormat  ///< The index declares a format other than the one this version reads.
};

/**
 * @brief Why an index could not be read.
 */
struct ToolchainIndexError {
    ToolchainIndexErrorKind kind = ToolchainIndexErrorKind::Malformed;
    std::string message;  ///< Where and what, e.g. "line 9: toolchains[0].status: ..."; for UnsupportedFormat, the declared format alone.
};

/**
 * @brief Parse the text of index.toml.
 *
 * The whole index is refused at the first fault, so no toolchain is chosen
 * from an index its maker got wrong. A key this version does not know is
 * passed over, which lets the maker add one without stopping older
 * versions; a status this version does not know is a fault, since it may
 * change whether a toolchain can be installed. An ingot is taken from a
 * registry when it names one and from a URL otherwise; holding both, or
 * neither, is a fault. Family, compiler, platform and arch are names that
 * can stand as one component of a path.
 *
 * A toolchain whose min_scrap_version is newer than @p scrapVersion is read
 * no further than its family, version and min_scrap_version, and is kept
 * disabled, so the maker can list one in a shape only a newer scrap knows.
 */
[[nodiscard]] std::expected<ToolchainIndex, ToolchainIndexError> parseToolchainIndex(std::string_view text, const ToolchainVersion& scrapVersion);

}  // namespace scrap::Toolchain
