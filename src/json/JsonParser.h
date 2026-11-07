#pragma once

#include "json/JsonValue.h"

#include <cstddef>
#include <expected>  // IWYU pragma: keep
#include <string>
#include <string_view>

namespace scrap::Json {

/**
 * @brief Why text is not one JSON value.
 */
struct ParseError {
    std::size_t offset = 0;  ///< Where in the text the problem was found.
    std::string reason;
};

/// How deeply arrays and objects may nest, so hostile text cannot exhaust the stack.
inline constexpr std::size_t NestingLimit = 64;

/**
 * @brief Read @p text as one JSON value (RFC 8259), with only whitespace around it.
 *
 * Strings come out as UTF-8 with their escapes resolved. An object that names
 * a member twice is refused, since readers disagree on which one counts. The
 * bytes of a string are passed through as they are.
 *
 * @return The value, or why the text is not one.
 */
[[nodiscard]] std::expected<Value, ParseError> parse(std::string_view text);

}  // namespace scrap::Json
