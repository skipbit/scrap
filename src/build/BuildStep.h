#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace scrap::Build {

/**
 * @brief What a step of the build does.
 */
enum class StepKind : std::uint8_t {
    Compile,
    Link
};

/**
 * @brief One program the build runs, with what it is said to do.
 */
struct BuildStep {
    StepKind kind = StepKind::Compile;
    std::string target;                  ///< The target the step builds.
    std::filesystem::path subject;       ///< What names the step: the source, or the executable it links.
    std::filesystem::path directory;     ///< Where the program runs, absolute.
    std::filesystem::path output;        ///< The file it writes, relative to the directory.
    std::vector<std::string> arguments;  ///< The command line, starting with the program.
};

/**
 * @brief How a step came to fail.
 */
enum class StepFailureKind : std::uint8_t {
    CannotCreateDirectory,  ///< The directory its output goes in could not be created.
    CannotStart,            ///< The program could not be started.
    Exited,                 ///< The program exited with a status other than 0.
    Signalled               ///< A signal stopped the program.
};

/**
 * @brief Why a step failed.
 */
struct StepFailure {
    StepFailureKind kind = StepFailureKind::Exited;
    std::filesystem::path path;  ///< The directory that could not be created, or the program that could not start.
    std::error_code code;        ///< The system's reason, for the first two kinds.
    int status = 0;              ///< The exit status, or the number of the signal.
};

/**
 * @brief What carrying out a step came to.
 */
struct StepResult {
    std::string output;                  ///< What the program wrote, both streams together.
    std::optional<StepFailure> failure;  ///< Why the step failed, or nothing when it succeeded.
};

}  // namespace scrap::Build
