#pragma once

#include <toml++/toml.hpp>

#include <string>

namespace scrap::Toolchain {

/**
 * @brief @p table written as TOML text, ending in a newline unless empty.
 *
 * Tables, keys and quotes come out in the shape toml++ gives them.
 */
[[nodiscard]] std::string tomlText(const toml::table& table);

}  // namespace scrap::Toolchain
