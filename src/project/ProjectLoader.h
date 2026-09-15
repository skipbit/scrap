#pragma once

#include "project/Manifest.h"
#include "project/ManifestError.h"

#include <expected>
#include <filesystem>
#include <variant>

namespace scrap::Project {

/**
 * @brief A project located on disk, with its manifest read.
 */
struct LoadedProject {
    std::filesystem::path root;  ///< Directory holding the manifest.
    Manifest manifest;
};

/**
 * @brief The directory to search from does not exist or is not a directory.
 */
struct NotADirectory {
    std::filesystem::path path;
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
using ProjectError = std::variant<NotADirectory, ProjectNotFound, ManifestError>;

/**
 * @brief Locate the project a directory belongs to and read its manifest.
 *
 * The search walks up from @p startDir as findProjectRoot() does. A start
 * that is not an existing directory is reported instead of searched from:
 * walking up from a mistyped path would load whichever project encloses it.
 *
 * @param startDir Directory to search from. Paths in the result are absolute,
 *                 normalised, and carry no trailing separator.
 * @return The project, or why it could not be loaded.
 */
[[nodiscard]] auto loadProject(const std::filesystem::path& startDir) -> std::expected<LoadedProject, ProjectError>;

}  // namespace scrap::Project
