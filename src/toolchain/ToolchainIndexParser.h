#pragma once

#include "toolchain/ToolchainIndex.h"

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
 * neither, is a fault.
 */
[[nodiscard]] std::expected<ToolchainIndex, ToolchainIndexError> parseToolchainIndex(std::string_view text);

}  // namespace scrap::Toolchain
