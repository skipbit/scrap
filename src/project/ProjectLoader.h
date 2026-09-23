#pragma once

#include "project/Manifest.h"
#include "project/ManifestError.h"

#include <expected>
#include <filesystem>
#include <string>
#include <variant>

namespace scrap::Project {

/**
 * @brief A project located on disk, with its manifest read.
 */
struct LoadedProject {
    std::filesystem::path root;  ///< Canonical directory holding the manifest.
    Manifest manifest;
};

/**
 * @brief The path to search from does not exist or is not a directory.
 */
struct NotADirectory {
    std::filesystem::path path;
};

/**
 * @brief The path to search from could not be examined, for example for lack
 *        of permission or because of a symbolic link loop.
 */
struct PathInaccessible {
    std::filesystem::path path;
    std::string reason;  ///< The operating system's description of the failure.
};

/**
 * @brief No manifest exists in the directory searched from or any parent.
 */
struct ProjectNotFound {
    std::filesystem::path startDir;
};

/**
 * @brief Why a project could not be loaded.
 */
using ProjectError = std::variant<NotADirectory, PathInaccessible, ProjectNotFound, ManifestError>;

/**
 * @brief Locate the project a directory belongs to and read its manifest.
 *
 * The filesystem resolves @p startDir first, so the start is an existing
 * directory exactly when the operating system agrees: "typo/.." fails as it
 * would in a shell, instead of walking up into whichever project encloses it.
 * The search then walks up from the canonical path as findProjectRoot() does.
 *
 * @param startDir Directory to search from. A relative path is taken against
 *                 the process's current directory.
 * @return The project, or why it could not be loaded. The root and the search
 *         start are canonical; a path that could not be resolved is reported
 *         absolute, as given.
 */
[[nodiscard]] std::expected<LoadedProject, ProjectError> loadProject(const std::filesystem::path& startDir);

}  // namespace scrap::Project
