#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

namespace scrap::Format {

/**
 * @brief A clang-format to run, and the version it reports.
 */
struct ClangFormat {
    std::filesystem::path program;  ///< Absolute path of the program.
    std::string version;            ///< As --version reports it, such as "18.1.3".
};

/**
 * @brief clang-format could not be started, or its output could not be read.
 */
struct ToolNotRun {
    std::error_code code;  ///< The system's reason.
};

/**
 * @brief clang-format ran and did not succeed.
 */
struct ToolFailed {
    std::optional<int> exitCode;  ///< Set when it exited with a status other than 0.
    std::optional<int> signal;    ///< Set when a signal stopped it.
    /// What it wrote to standard output and standard error. A run that reads
    /// standard output alone runs again to read both; when the second run
    /// does not fail the same way, this holds the first run's standard output.
    std::string output;
};

/**
 * @brief clang-format succeeded, and the file could not be read to compare
 *        with what it wrote.
 */
struct FileUnreadable { };

/**
 * @brief Why clang-format did not do what was asked.
 */
using ToolFailure = std::variant<ToolNotRun, ToolFailed, FileUnreadable>;

/**
 * @brief Whether formatting a file would change it.
 */
enum class FileState : std::uint8_t {
    Formatted,   ///< Formatting leaves it as it is.
    Unformatted  ///< Formatting would change it.
};

/**
 * @brief The clang-format found first on @p searchPaths, or nothing.
 */
[[nodiscard]] std::optional<std::filesystem::path> findClangFormat(const std::vector<std::filesystem::path>& searchPaths);

/**
 * @brief Ask @p program for its version.
 *
 * The version is the word after "version" on the first line --version writes
 * that has one, else the first line that is not empty, else "unknown". The
 * program is kept as an absolute path, since a format runs it from the
 * project root.
 */
[[nodiscard]] std::expected<ClangFormat, ToolFailure> describeClangFormat(const std::filesystem::path& program);

/**
 * @brief Format @p file in place.
 *
 * @param tool The clang-format to run.
 * @param projectRoot Directory clang-format runs in.
 * @param file File to format, relative to @p projectRoot.
 * @param style Passed to --style as written; nothing leaves clang-format to
 *        find a .clang-format.
 */
[[nodiscard]] std::expected<void, ToolFailure> formatFile(const ClangFormat& tool,
                                                          const std::filesystem::path& projectRoot,
                                                          const std::filesystem::path& file,
                                                          const std::optional<std::string>& style);

/**
 * @brief Whether formatting @p file would change it, leaving the file as it is.
 *
 * clang-format writes the formatted text, which is compared with the file.
 * The parameters are those of formatFile().
 */
[[nodiscard]] std::expected<FileState, ToolFailure> checkFile(const ClangFormat& tool,
                                                              const std::filesystem::path& projectRoot,
                                                              const std::filesystem::path& file,
                                                              const std::optional<std::string>& style);

}  // namespace scrap::Format
