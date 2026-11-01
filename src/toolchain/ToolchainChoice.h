#pragma once

#include "toolchain/ToolchainSpecifier.h"

#include <expected>
#include <string>
#include <string_view>
#include <variant>

namespace scrap::Toolchain {

/**
 * @brief The compiler found on the system, named by the word system.
 */
struct SystemToolchain { };

/**
 * @brief The toolchain a user chose: the system's, or those a specifier
 *        names.
 */
using ToolchainChoice = std::variant<SystemToolchain, ToolchainSpecifier>;

/**
 * @brief Read @p text as the word system or as a specifier.
 *
 * @return The choice, or why @p text is not a specifier when it is not
 *         system either.
 */
[[nodiscard]] std::expected<ToolchainChoice, std::string> parseToolchainChoice(std::string_view text);

}  // namespace scrap::Toolchain
