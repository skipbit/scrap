#pragma once

#include "tool/ExternalTool.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace scrap::Format {

/// The name clang-format is looked for by, and the messages name it by.
inline constexpr std::string_view ClangFormatName = "clang-format";

/**
 * @brief clang-format succeeded, and the file could not be read to compare
 *        with what it wrote.
 */
struct FileUnreadable { };

/**
 * @brief Why a check could not tell whether formatting would change a file.
 */
using CheckFailure = std::variant<Tool::ToolNotRun, Tool::ToolFailed, FileUnreadable>;

/**
 * @brief Whether formatting a file would change it.
 */
enum class FileState : std::uint8_t {
    Formatted,   ///< Formatting leaves it as it is.
    Unformatted  ///< Formatting would change it.
};

/**
 * @brief Format @p file in place.
 *
 * @param tool The clang-format to run.
 * @param projectRoot Directory clang-format runs in.
 * @param file File to format, relative to @p projectRoot.
 * @param style Passed to --style as written; nothing leaves clang-format to
 *        find a .clang-format.
 */
[[nodiscard]] std::expected<void, Tool::ToolFailure> formatFile(const Tool::ExternalTool& tool,
                                                                const std::filesystem::path& projectRoot,
                                                                const std::filesystem::path& file,
                                                                const std::optional<std::string>& style);

/**
 * @brief Whether formatting @p file would change it, leaving the file as it is.
 *
 * clang-format writes the formatted text, which is compared with the file.
 * The parameters are those of formatFile().
 */
[[nodiscard]] std::expected<FileState, CheckFailure> checkFile(const Tool::ExternalTool& tool,
                                                               const std::filesystem::path& projectRoot,
                                                               const std::filesystem::path& file,
                                                               const std::optional<std::string>& style);

}  // namespace scrap::Format
