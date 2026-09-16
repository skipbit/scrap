#pragma once

#include <filesystem>
#include <string>

namespace scrap::Project {

/**
 * @brief One file a project template places in a new project.
 */
struct TemplateFile {
    std::filesystem::path path;  ///< Relative to the project root.
    std::string content;
};

}  // namespace scrap::Project
