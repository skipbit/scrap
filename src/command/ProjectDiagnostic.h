#pragma once

#include "project/ProjectLoader.h"

#include <string>

namespace scrap::Command {

/**
 * @brief Describe why a project could not be loaded, and what to do next.
 *
 * The first line states the cause: "error: ..." for a missing project or
 * directory, and the compiler-style line from scrap::Project::describe() for
 * a manifest error. A "hint: ..." line with the next step follows.
 *
 * @param error Error returned by scrap::Project::loadProject().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderProjectError(const Project::ProjectError& error) -> std::string;

}  // namespace scrap::Command
