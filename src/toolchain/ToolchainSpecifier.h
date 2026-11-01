#pragma once

#include "toolchain/ToolchainVersion.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Toolchain {

/**
 * @brief Which toolchains a user means, written <family>[@<version>].
 *
 * The version may be cut short to name a series: gcc@15 names every 15.x.y.
 * It never names an OS or an architecture, which are always the host's.
 */
struct ToolchainSpecifier {
    std::string family;
    std::optional<ToolchainVersion> version;  ///< Nothing names every version of the family.

    /** Whether the toolchain @p family @p version is one this names. */
    [[nodiscard]] bool matches(std::string_view family, const ToolchainVersion& version) const;
};

/**
 * @brief Read @p text as a specifier.
 *
 * The word system is not a specifier; a caller that takes it tells it apart
 * before reading the rest.
 *
 * @return The specifier, or why @p text is not one.
 */
[[nodiscard]] std::expected<ToolchainSpecifier, std::string> parseToolchainSpecifier(std::string_view text);

}  // namespace scrap::Toolchain
