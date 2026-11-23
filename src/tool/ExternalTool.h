#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

namespace scrap::Tool {

/**
 * @brief A program scrap runs to do its work, and the version it reports.
 */
struct ExternalTool {
    std::string name;               ///< The tool as the messages name it, such as "clang-format".
    std::filesystem::path program;  ///< Absolute path of the program.
    std::string version;            ///< As --version reports it, such as "18.1.3".
};

/**
 * @brief The program could not be started, or its output could not be read.
 */
struct ToolNotRun {
    std::error_code code;  ///< The system's reason.
};

/**
 * @brief The program ran and did not succeed.
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
 * @brief Why the program did not do what was asked.
 */
using ToolFailure = std::variant<ToolNotRun, ToolFailed>;

/**
 * @brief The program named @p name found first on @p searchPaths, as an
 *        absolute path, or nothing.
 */
[[nodiscard]] std::optional<std::filesystem::path> findTool(std::string_view name, const std::vector<std::filesystem::path>& searchPaths);

/**
 * @brief Ask @p program, the tool named @p name, for its version.
 *
 * The version is the word after "version" on the first line --version writes
 * that has one, else the first line that is not empty, else "unknown". The
 * program is kept as an absolute path, since it may run from another
 * directory.
 */
[[nodiscard]] std::expected<ExternalTool, ToolFailure> describeTool(std::string_view name, const std::filesystem::path& program);

/**
 * @brief Run @p arguments in @p directory and return everything it wrote,
 *        standard output and standard error together, when it succeeded.
 */
[[nodiscard]] std::expected<std::string, ToolFailure> runTool(const std::vector<std::string>& arguments, const std::filesystem::path& directory);

/**
 * @brief Run @p arguments in @p directory and return what it wrote to
 *        standard output when it succeeded.
 *
 * Standard output and standard error cannot be read apart, so when it does
 * not succeed a second run reads both, standard error holding why.
 */
[[nodiscard]] std::expected<std::string, ToolFailure> standardOutputOf(const std::vector<std::string>& arguments, const std::filesystem::path& directory);

}  // namespace scrap::Tool
