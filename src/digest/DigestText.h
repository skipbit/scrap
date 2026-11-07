#pragma once

#include <string>
#include <string_view>

namespace scrap::Digest {

/// What precedes the hexadecimal digits in the text of a SHA-256 digest.
inline constexpr std::string_view Sha256Prefix = "sha256:";

/**
 * @brief Whether @p text is 64 lowercase hexadecimal digits.
 */
[[nodiscard]] bool isSha256Hex(std::string_view text);

/**
 * @brief Whether @p text is Sha256Prefix followed by 64 lowercase hexadecimal digits.
 */
[[nodiscard]] bool isSha256Digest(std::string_view text);

/**
 * @brief The text of the digest whose hexadecimal digits are @p hex.
 */
[[nodiscard]] std::string sha256Digest(std::string_view hex);

}  // namespace scrap::Digest
