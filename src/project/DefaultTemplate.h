#pragma once

#include "project/TemplateFile.h"

#include <string_view>
#include <vector>

namespace scrap::Project {

/**
 * @brief The files of the template built into scrap.
 *
 * A manifest naming the package, and a src/main.cpp that the default layout
 * turns into an executable of the same name.
 *
 * @param projectName A name accepted by isValidProjectName().
 * @return Files to create, relative to the project root.
 */
[[nodiscard]] auto defaultTemplateFiles(std::string_view projectName) -> std::vector<TemplateFile>;

}  // namespace scrap::Project
