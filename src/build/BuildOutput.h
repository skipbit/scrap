#pragma once

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string_view>
#include <system_error>

namespace scrap::Build {

/// Directory, relative to the project root, that holds the output of every build.
inline constexpr std::string_view OutputDirectory = "build";

/**
 * @brief What removing the build output came to.
 */
enum class OutputRemoval : std::uint8_t {
    Removed,      ///< The directory, or the link standing in its place, is gone.
    NothingThere  ///< There was nothing at the path to remove.
};

/**
 * @brief Why the build output was not removed.
 */
enum class OutputRemovalProblem : std::uint8_t {
    CannotInspect,  ///< What is at the path could not be examined.
    NotADirectory,  ///< Something other than a directory or a link is there.
    CannotRemove    ///< Removing it failed part way or not at all.
};

/**
 * @brief The build output could not be removed.
 */
struct OutputRemovalFailure {
    OutputRemovalProblem problem = OutputRemovalProblem::CannotRemove;
    std::filesystem::path path;  ///< The path that was to be removed.
    std::error_code code;        ///< What the operating system reported, empty for NotADirectory.
};

/**
 * @brief Remove the directory that holds the output of every build.
 *
 * A directory is removed with everything below it; links below it are
 * removed without following them. A symbolic link at the path is removed
 * itself, and what it points to is left alone. Anything else at the path was
 * not written by a build, so it is left in place and reported.
 *
 * @param directory The directory to remove, absolute.
 * @return What was done, or why nothing, or not all, was removed.
 */
[[nodiscard]] std::expected<OutputRemoval, OutputRemovalFailure> removeOutput(const std::filesystem::path& directory);

}  // namespace scrap::Build
