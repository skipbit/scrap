#pragma once

#include "project/Manifest.h"

#include <filesystem>
#include <vector>

namespace scrap::Project {

/**
 * @brief A target and the sources a build compiles for it.
 */
struct TargetSources {
    Target target;
    std::vector<std::filesystem::path> sources;  ///< Relative to the project root, sorted.
};

/**
 * @brief Decide which sources each target is built from.
 *
 * Every source below src/ belongs to a target, except the entry points of the
 * other targets: two executables share the code beside them and differ in the
 * file that starts each one. A target keeps its own entry point even when the
 * scan does not reach it, since a declaration states what to build wherever
 * the file sits.
 *
 * A source is recognised by its extension: .cpp, .cc or .cxx. The list comes
 * back sorted, so a build reads the same sources whatever order the file
 * system reports its entries in.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param targets Targets to build, as resolveTargets() returned them.
 * @return Each target with its sources, in the order the targets were given.
 */
[[nodiscard]] auto collectSources(const std::filesystem::path& projectRoot,
                                  const std::vector<Target>& targets) -> std::vector<TargetSources>;

}  // namespace scrap::Project
