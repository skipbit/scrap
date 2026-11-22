#pragma once

#include "command/ProjectPaths.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief Describe why the project of the path arguments could not be loaded,
 *        and what to do next.
 *
 * A project that could not be loaded is described as for
 * renderProjectError().
 *
 * @param error Error returned by loadProjectWithPaths().
 * @param command The command the paths were given to, such as "format".
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderProjectPathsError(const ProjectPathsError& error, std::string_view command);

/**
 * @brief Say that a named file is left out, as the command does not cover it.
 *
 * @param file The file, relative to the project root.
 * @param command The command it was given to, such as "format".
 * @return A "warning: ..." line ending in a newline.
 */
[[nodiscard]] std::string renderNotCovered(const std::filesystem::path& file, std::string_view command);

}  // namespace scrap::Command
