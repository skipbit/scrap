#pragma once

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace scrap::File {

/**
 * @brief The step at which reading a file failed.
 */
enum class ReadFailure : std::uint8_t {
    CannotOpen,  ///< It is not a regular file, or could not be opened.
    CannotRead   ///< Its size could not be learned, or less than that arrived.
};

/**
 * @brief Read the whole of @p file, distinguishing a failure to read from an
 *        empty file.
 *
 * Streaming through a stringstream cannot tell those apart: inserting from a
 * streambuf reports on the destination, never on the source, so a directory
 * opened as a file and a zero-byte file both arrive as empty text. Reading a
 * known length and comparing what arrived keeps them apart.
 *
 * @return The bytes of the file, or the step that failed.
 */
[[nodiscard]] std::expected<std::string, ReadFailure> readWholeFile(const std::filesystem::path& file);

/**
 * @brief Put @p content at @p file, whole.
 *
 * The content is staged beside the file (see StagedFile) and then renamed over
 * it, so a reader sees either the previous file or the new one whole.
 *
 * @return Nothing on success, or what the operating system reported.
 */
[[nodiscard]] std::error_code replaceFile(const std::filesystem::path& file, std::string_view content);

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

/**
 * @brief Create @p file and put @p content in it, whole.
 *
 * The call fails when anything is at the path, a symbolic link included,
 * which is then neither followed nor truncated. The file is created readable
 * by everyone and writable by its owner, before the process umask, and is
 * removed again when writing it fails.
 *
 * @return Nothing on success, file_exists when something is at the path, or
 *         what the operating system reported.
 */
[[nodiscard]] std::error_code writeNewFile(const std::filesystem::path& file, std::string_view content);

}  // namespace scrap::File
