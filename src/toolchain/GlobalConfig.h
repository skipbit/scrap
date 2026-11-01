#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Toolchain {

/**
 * @brief Read the global default toolchain from the text of config.toml.
 *
 * The default is the toolchain key of the defaults table. Keys this version
 * does not know are passed over, since a later version may write them.
 * Empty text holds no default.
 *
 * @return The default as written, which is system or a specifier, or
 *         nothing when none is set; or where and why the text cannot be
 *         read, e.g. "line 2: defaults.toolchain: must be a string".
 */
[[nodiscard]] std::expected<std::optional<std::string>, std::string> readDefaultToolchain(std::string_view text);

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
[[nodiscard]] std::expected<std::string, std::string> writeDefaultToolchain(std::string_view text, const std::optional<std::string>& toolchain);

}  // namespace scrap::Toolchain
