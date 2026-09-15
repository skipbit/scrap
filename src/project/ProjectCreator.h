#pragma once

#include "project/TemplateFile.h"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
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
 * @brief Builds the files of a project from its name.
 */
using TemplateFiles = std::function<std::vector<TemplateFile>(std::string_view projectName)>;

/// The longest project name, in characters, isValidProjectName() accepts.
inline constexpr std::size_t MaxProjectNameLength = 64;

/**
 * @brief Whether @p name can name a new project.
 *
 * A valid name is an ASCII letter followed by ASCII letters, digits, '-' and
 * '_', at most MaxProjectNameLength characters long. The name becomes a
 * directory, the package name and the executable name, so it is held to what
 * a shell and a file system take without quoting, well within any file name
 * length limit.
 * The rule is stricter than what scrap.toml accepts, and loosening it later
 * keeps every project created under it valid.
 */
[[nodiscard]] auto isValidProjectName(std::string_view name) -> bool;

/**
 * @brief Create a project directory and write the template's files into it.
 *
 * The directory is created in a single step that fails when anything already
 * exists at its path, so an existing directory stays untouched. When a later
 * step fails, the directory this call created is removed again.
 *
 * @param parentDir Directory to create the project in.
 * @param name Project name, checked with isValidProjectName().
 * @param templateFiles Called with @p name only once the name is valid; returns
 *                      the files to write, relative to the project root.
 * @return The absolute project root, or why the project could not be created.
 */
[[nodiscard]] auto
createProject(const std::filesystem::path& parentDir,
              std::string_view name,
              const TemplateFiles& templateFiles) -> std::expected<std::filesystem::path, CreateProjectError>;

}  // namespace scrap::Project
