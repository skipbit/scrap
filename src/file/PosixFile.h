#pragma once

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string_view>
#include <system_error>

namespace scrap::File {

/**
 * @brief The failure the last system call reported.
 */
[[nodiscard]] std::error_code lastFailure();

/**
 * @brief Create @p file for writing, failing when anything is at the path.
 *
 * A symbolic link at the path is neither followed nor truncated. The file is
 * created readable by everyone and writable by its owner, before the process
 * umask.
 *
 * @return A descriptor the caller closes, file_exists when something is at the
 *         path, or what the operating system reported.
 */
[[nodiscard]] std::expected<int, std::error_code> openNewFile(const std::filesystem::path& file);

/**
 * @brief Write the whole of @p content to @p descriptor.
 *
 * @return Nothing on success, or what the operating system reported.
 */
[[nodiscard]] std::error_code writeAll(int descriptor, std::string_view content);

}  // namespace scrap::File
