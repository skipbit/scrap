#pragma once

#include "project/Manifest.h"

#include <filesystem>
#include <vector>

namespace scrap::Project {

/**
 * @brief Decide which targets a project builds.
 *
 * A manifest that declares [[bin]] or [[lib]] says exactly what to build,
 * including when it declares an empty list. Only a manifest that declares
 * neither is read against the default layout: src/main.cpp becomes a single
 * executable named after the package. A project with neither declaration nor
 * src/main.cpp has nothing to build.
 *
 * A declared entry point is returned whether or not the file is there. The
 * layout is consulted to decide whether a target can be inferred at all,
 * which is a different question from whether a declared source exists; a
 * declared path that is missing is reported by the build, which is where the
 * compiler's own diagnostic belongs.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param manifest Parsed manifest.
 * @return Targets to build, empty when there are none.
 */
[[nodiscard]] auto resolveTargets(const std::filesystem::path& projectRoot,
                                  const Manifest& manifest) -> std::vector<Target>;

}  // namespace scrap::Project
