#pragma once

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace scrap::Process {

/**
 * @brief Which of a program's output streams are read back.
 */
enum class OutputCapture : std::uint8_t {
    StandardOutput,  ///< Standard output is read; standard error is discarded.
    Combined         ///< Both streams are read together, in the order written.
};

/**
 * @brief How a program that ran came to an end, and what it wrote.
 *
 * Exactly one of exitCode and signal is set.
 */
struct Completion {
    std::optional<int> exitCode;  ///< Set when the program exited.
    std::optional<int> signal;    ///< Set when a signal stopped the program.
    std::string output;           ///< What the program wrote to the streams read back.
};

/**
 * @brief Run a program to completion and read back its output.
 *
 * The program is started directly, without a shell, so each argument reaches
 * it as written. It inherits the environment, reads nothing from standard
 * input, and stays in the caller's process group, so an interrupt from the
 * terminal reaches it as well.
 *
 * @param arguments The command line; the first is the path of the program.
 * @param workingDirectory Directory the program runs in, or empty to run it
 *        where the caller runs.
 * @param capture Which output streams are read back.
 * @return How the program ended, or why it could not be started.
 */
[[nodiscard]] auto runProgram(const std::vector<std::string>& arguments,
                              const std::filesystem::path& workingDirectory,
                              OutputCapture capture) -> std::expected<Completion, std::error_code>;

}  // namespace scrap::Process
