#pragma once

#include "project/Manifest.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <vector>

namespace scrap::Project {

/**
 * @brief A target and the sources a build compiles for it.
 */
struct TargetSources {
    Target target;
    std::vector<std::filesystem::path> sources;  ///< Relative to the project root, normalized and sorted.
};

/**
 * @brief The source directory could not be read.
 */
struct SourceScanFailure {
    std::filesystem::path directory;  ///< Directory being read when it failed.
    std::string reason;               ///< The operating system's description of the failure.
};

/**
 * @brief Decide which sources each target is built from.
 *
 * Every source below src/ belongs to a target, except the entry points of the
 * other targets: two executables share the code beside them and differ in the
 * file that starts each one. A target keeps its own entry point wherever it
 * sits, since a declaration states what to build whether or not the default
 * layout expects the file there. Paths are compared once normalized, so
 * "./src/main.cpp" and "src/main.cpp" name the same file.
 *
 * A source is recognised by its extension, spelled in lower case: .cpp, .cc
 * or .cxx. The list comes back sorted, so a build reads the same sources
 * whatever order the file system reports its entries in.
 *
 * A project without src/ is not a failure, since a target can declare an
 * entry point anywhere. A directory that cannot be read is reported, rather
 * than leaving sources out of an artifact without a word.
 *
 * Targets of different kinds are not separated yet: an executable beside a
 * library is given the library's sources except its entry point. Whether an
 * executable compiles a library's sources or links the library they produce
 * belongs with the step that links them.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param targets Targets to build, as resolveTargets() returned them.
 * @return Each target with its sources, in the order the targets were given,
 *         or the directory that could not be read.
 */
[[nodiscard]] auto collectSources(const std::filesystem::path& projectRoot, const std::vector<Target>& targets)
    -> std::expected<std::vector<TargetSources>, SourceScanFailure>;

}  // namespace scrap::Project
