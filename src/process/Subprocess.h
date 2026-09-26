#pragma once

#include <chrono>
#include <cstddef>
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
 * @brief Which process group a program runs in.
 */
enum class ProcessGroup : std::uint8_t {
    Caller,  ///< The caller's, so an interrupt from the terminal reaches it as well.
    Own      ///< One of its own, so it can be stopped with everything it started.
};

/**
 * @brief How a program is run and how long it is waited for.
 */
struct RunOptions {
    std::filesystem::path workingDirectory;                 ///< Where it runs, or empty to run it where the caller runs.
    OutputCapture capture = OutputCapture::StandardOutput;  ///< Which output streams are read back.
    ProcessGroup group = ProcessGroup::Caller;              ///< Which process group it runs in.
    std::optional<std::chrono::milliseconds> timeout;       ///< How long it may run, or none to wait until it ends.
    std::optional<std::size_t> outputLimit;                 ///< Most bytes read back, or none to read everything.
};

/**
 * @brief How a program that ran came to an end, and what it wrote.
 *
 * Exactly one of exitCode and signal is set.
 */
struct Completion {
    std::optional<int> exitCode;  ///< Set when the program exited.
    std::optional<int> signal;    ///< Set when a signal stopped the program.
    bool timedOut = false;        ///< Set when the program was stopped because its time ran out.
    std::string output;           ///< What the program wrote to the streams read back.
};

/**
 * @brief Run a program to completion and read back its output.
 *
 * The program is started directly, without a shell, so each argument reaches
 * it as written. It inherits the environment and reads nothing from standard
 * input.
 *
 * When a timeout is given and the program is still running when it passes,
 * the program is killed, together with its process group when it has one of
 * its own. Output past the limit is not read, so a program that keeps writing
 * sees its output closed.
 *
 * @param arguments The command line; the first is the path of the program.
 * @param options How the program is run.
 * @return How the program ended, or why it could not be started or read.
 */
[[nodiscard]] std::expected<Completion, std::error_code> runProgram(const std::vector<std::string>& arguments, const RunOptions& options);

}  // namespace scrap::Process
