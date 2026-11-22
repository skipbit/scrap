#pragma once

#include "command/ProjectPaths.h"

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

}  // namespace scrap::Command
