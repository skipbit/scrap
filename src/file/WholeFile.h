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
 * The content is written beside the file under a name no other file holds
 * and then renamed over it, so a reader sees either the previous file or the
 * new one whole, and two processes writing at once each write a file of
 * their own. The name carries this process's id and a count; creating it
 * exclusively moves on to the next count when another process, one in
 * another container with the same id among them, already holds it. The
 * staged file is removed when a later step fails. The file is created
 * readable by everyone and writable by its owner, before the process umask.
 *
 * @return Nothing on success, or what the operating system reported.
 */
[[nodiscard]] std::error_code replaceFile(const std::filesystem::path& file, std::string_view content);

/**
 * @brief Write the whole of @p content to @p descriptor.
 *
 * A write interrupted by a signal is repeated, and a write that reports
 * nothing written is a failure, since repeating it would make no progress.
 * The descriptor is left open.
 *
 * @return Nothing on success, or what the operating system reported.
 */
[[nodiscard]] std::error_code writeAll(int descriptor, std::string_view content);

}  // namespace scrap::File
