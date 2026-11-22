#include "command/ProjectPaths.h"

#include "command/ProjectArgument.h"
#include "project/FileSelection.h"
#include "project/ProjectLoader.h"
#include "project/ProjectLocator.h"

#include <cstddef>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * @brief A path argument as the filesystem resolves it.
 */
struct ResolvedPath {
    std::filesystem::path real;  ///< Canonical.
    bool isDirectory = false;
};

/// Whether @p code says that a path names nothing.
bool namesNothing(const std::error_code& code)
{
    return (code == std::errc::no_such_file_or_directory) || (code == std::errc::not_a_directory);
}

/// The failure of a path that exists and could not be examined.
ProjectPathsError inaccessible(const std::filesystem::path& path, const std::error_code& code)
{
    return Project::ProjectError{ Project::PathInaccessible{ .path = path, .reason = code.message() } };
}

/**
 * @brief @p given taken against @p workingDirectory and resolved through the
 *        filesystem.
 */
std::expected<ResolvedPath, ProjectPathsError> resolve(const std::filesystem::path& workingDirectory, const std::string& given)
{
    if (given.empty()) {
        return std::unexpected(EmptyPathArgument{});
    }
    const std::filesystem::path absolute = workingDirectory / given;
    std::error_code code;
    std::filesystem::path real = std::filesystem::canonical(absolute, code);
    if (code) {
        if (namesNothing(code)) {
            return std::unexpected(PathMissing{ .path = given });
        }
        return std::unexpected(inaccessible(given, code));
    }
    const bool isDirectory = std::filesystem::is_directory(real, code);
    if (code) {
        return std::unexpected(inaccessible(given, code));
    }
    return ResolvedPath{ .real = std::move(real), .isDirectory = isDirectory };
}

/// The directory to search for the project of @p path from.
std::filesystem::path searchStart(const ResolvedPath& path)
{
    return path.isDirectory ? path.real : path.real.parent_path();
}

}  // anonymous namespace

std::expected<ProjectWithPaths, ProjectPathsError> loadProjectWithPaths(const std::filesystem::path& workingDirectory, const std::vector<std::string>& paths)
{
    std::vector<ResolvedPath> resolved;
    for (const std::string& given : paths) {
        auto path = resolve(workingDirectory, given);
        if (! path.has_value()) {
            return std::unexpected(std::move(path.error()));
        }
        resolved.push_back(std::move(*path));
    }

    auto project = Project::loadProject(resolved.empty() ? workingDirectory : searchStart(resolved.front()));
    if (! project.has_value()) {
        return std::unexpected(ProjectPathsError{ std::move(project.error()) });
    }

    ProjectWithPaths loaded{ .project = std::move(*project), .requested = {} };
    for (std::size_t index = 0; index < resolved.size(); ++index) {
        const ResolvedPath& path = resolved[index];
        if (Project::findProjectRoot(searchStart(path)) != loaded.project.root) {
            return std::unexpected(PathOutsideProject{ .path = paths[index], .projectRoot = loaded.project.root });
        }
        loaded.requested.push_back(Project::RequestedPath{ .path = path.real.lexically_relative(loaded.project.root), .isDirectory = path.isDirectory });
    }
    return loaded;
}

}  // namespace scrap::Command
