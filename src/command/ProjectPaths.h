#pragma once

#include "command/ProjectArgument.h"
#include "project/FileSelection.h"
#include "project/ProjectLoader.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace scrap::Command {

/**
 * @brief A path argument that names nothing.
 */
struct PathMissing {
    std::filesystem::path path;  ///< As given.
};

/**
 * @brief A path argument outside the project the first one belongs to.
 */
struct PathOutsideProject {
    std::filesystem::path path;         ///< As given.
    std::filesystem::path projectRoot;  ///< The project of the first path.
};

/**
 * @brief Why the project of the path arguments could not be loaded.
 */
using ProjectPathsError = std::variant<EmptyPathArgument, Project::ProjectError, PathMissing, PathOutsideProject>;

/**
 * @brief A project, and the paths in it the command line names.
 */
struct ProjectWithPaths {
    Project::LoadedProject project;
    std::vector<Project::RequestedPath> requested;  ///< In the order given.
};

/**
 * @brief Load the project that the files and directories a command names
 *        belong to.
 *
 * Each path is taken against @p workingDirectory and resolved through the
 * filesystem, so a path spelled in another letter case on a filesystem that
 * ignores case, or through a symbolic link, names the file it reaches. The
 * project is searched for from the first path, or from the directory holding
 * it when it names a file, and from @p workingDirectory when no path is
 * given. Every other path must belong to the same project.
 *
 * @param workingDirectory Directory the command was invoked in.
 * @param paths Paths the command line names, as given.
 * @return The project and the paths relative to its root, or why they could
 *         not be taken.
 */
[[nodiscard]] std::expected<ProjectWithPaths, ProjectPathsError> loadProjectWithPaths(const std::filesystem::path& workingDirectory,
                                                                                      const std::vector<std::string>& paths);

}  // namespace scrap::Command
