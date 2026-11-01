#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Toolchain {

/**
 * @brief Where and why the text of config.toml cannot be read.
 */
struct GlobalConfigFault {
    std::uint32_t line = 0;    ///< One-based.
    std::uint32_t column = 0;  ///< One-based.
    std::string key;           ///< Dotted key, e.g. "defaults.toolchain"; empty when the text is not TOML.
    std::string message;
};

/**
 * @brief Read the global default toolchain from the text of config.toml.
 *
 * The default is the toolchain key of the defaults table. Keys this version
 * does not know are passed over, since a later version may write them.
 * Empty text holds no default.
 *
 * @return The default as written, which is system or a specifier, or
 *         nothing when none is set; or where and why the text cannot be
 *         read.
 */
[[nodiscard]] std::expected<std::optional<std::string>, GlobalConfigFault> readDefaultToolchain(std::string_view text);

/**
 * @brief The text of config.toml with the global default toolchain set to
 *        @p toolchain, or removed when it is nothing.
 *
 * Every other key is kept, but comments are not, and tables, keys and
 * quotes come back in the shape the TOML writer gives them. A defaults
 * table left empty is removed. @p toolchain is written as given; the caller
 * passes system or a specifier.
 *
 * @return The new text, or why @p text cannot be read; text that cannot be
 *         read is never written over, since what it holds would be lost.
 */
[[nodiscard]] std::expected<std::string, GlobalConfigFault> writeDefaultToolchain(std::string_view text, const std::optional<std::string>& toolchain);

}  // namespace scrap::Toolchain
