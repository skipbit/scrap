#pragma once

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <system_error>

namespace scrap::Digest {

/**
 * @brief SHA-256 of the content of @p file, in lowercase hexadecimal.
 *
 * @return The digest, or what the operating system reported.
 */
[[nodiscard]] std::expected<std::string, std::error_code> sha256OfFile(const std::filesystem::path& file);

}  // namespace scrap::Digest
