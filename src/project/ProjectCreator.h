#pragma once

#include "project/TemplateFile.h"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace scrap::Project {

/**
 * @brief The name cannot name a project.
 */
struct InvalidProjectName {
    std::string name;  ///< As given; empty when the name was empty.
};

/**
 * @brief Something already exists where the project would be created.
 */
struct PathExists {
    std::filesystem::path path;
};

/**
 * @brief A directory or file of the project could not be created.
 */
struct CannotCreate {
    std::filesystem::path path;
    std::string reason;  ///< The operating system's description of the failure.
};

/**
 * @brief Why a project could not be created.
 */
using CreateProjectError = std::variant<InvalidProjectName, PathExists, CannotCreate>;

/**
 * @brief Whether @p name can name a new project.
 *
 * A valid name is an ASCII letter followed by ASCII letters, digits, '-' and
 * '_'. The name becomes a directory, the package name and the executable
 * name, so it is held to what a shell and a file system take without quoting.
 * The rule is stricter than what scrap.toml accepts, and loosening it later
 * keeps every project created under it valid.
 */
[[nodiscard]] auto isValidProjectName(std::string_view name) -> bool;

/**
 * @brief Create a project directory and write @p files into it.
 *
 * The directory is created in a single step that fails when anything already
 * exists at its path, so an existing directory stays untouched. When a later
 * step fails, the directory this call created is removed again.
 *
 * @param parentDir Directory to create the project in.
 * @param name Project name, checked with isValidProjectName().
 * @param files Files to write, relative to the project root.
 * @return The absolute project root, or why the project could not be created.
 */
[[nodiscard]] auto
createProject(const std::filesystem::path& parentDir,
              std::string_view name,
              const std::vector<TemplateFile>& files) -> std::expected<std::filesystem::path, CreateProjectError>;

}  // namespace scrap::Project
