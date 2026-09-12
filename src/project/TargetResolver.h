#pragma once

#include "project/Manifest.h"

#include <filesystem>
#include <vector>

namespace scrap::Project {

/**
 * @brief Decide which targets a project builds.
 *
 * A manifest that declares [[bin]] or [[lib]] tables says exactly what to
 * build. One that declares neither is read against the default layout:
 * src/main.cpp becomes a single executable named after the package. A project
 * with neither declaration nor src/main.cpp has nothing to build.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param manifest Parsed manifest.
 * @return Targets to build, empty when there are none.
 */
[[nodiscard]] auto resolveTargets(const std::filesystem::path& projectRoot,
                                  const Manifest& manifest) -> std::vector<Target>;

}  // namespace scrap::Project
