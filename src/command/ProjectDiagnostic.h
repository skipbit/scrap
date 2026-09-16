#pragma once

#include "project/ProjectCreator.h"
#include "project/ProjectLoader.h"

#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief Describe why a project could not be loaded, and what to do next.
 *
 * The first line states the cause: "error: ..." for a path or a missing
 * project, and the compiler-style line from scrap::Project::describe() for a
 * manifest. A "hint: ..." line with the next step follows.
 *
 * @param error Error returned by scrap::Project::loadProject().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderProjectError(const Project::ProjectError& error) -> std::string;

/**
 * @brief Describe an explicitly empty path argument, and what to do next.
 *
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderEmptyPathArgument() -> std::string;

/**
 * @brief Describe why a project could not be created, and what to do next.
 *
 * @param error Error returned by scrap::Project::createProject().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderCreateProjectError(const Project::CreateProjectError& error) -> std::string;

/**
 * @brief A path made safe to print.
 *
 * A path carries a directory the user named, which can hold any byte, so
 * control characters and a backslash become \xNN while letters outside ASCII
 * stay as they are. Every message that shows a path passes it through here.
 *
 * @param path Path to render.
 * @return The path as text for a terminal.
 */
[[nodiscard]] auto printablePath(const std::filesystem::path& path) -> std::string;

}  // namespace scrap::Command
