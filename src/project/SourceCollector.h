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
 * Without a library, every source below src/ belongs to each executable,
 * except the entry points of the other executables: two executables share the
 * code beside them and differ in the file that starts each one. A target keeps its own entry point wherever it
 * sits, since a declaration states what to build whether or not the default
 * layout expects the file there. Paths are compared once normalized, so
 * "./src/main.cpp" and "src/main.cpp" name the same file.
 *
 * A source is recognised by its extension, spelled in lower case: .cpp, .cc
 * or .cxx. The list comes back sorted, so a build reads the same sources
 * whatever order the file system reports its entries in.
 *
 * A project without src/ is not a failure, since a target can declare an
 * entry point anywhere. A directory that exists and cannot be read, or whose
 * state cannot be determined, is reported with the path that failed. A
 * symbolic link to a directory is listed and not followed, so sources below
 * it are left out; a link to a file is read as the file it names.
 *
 * A project with a library gives the sources below src/ to the library, except
 * the entry points of the executables, and adds the file the library names,
 * if any. Each executable is then built from its entry point alone and links
 * the library.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param targets Targets to build, as resolveTargets() returned them.
 * @return Each target with its sources, in the order the targets were given,
 *         or the directory that could not be read.
 */
[[nodiscard]] std::expected<std::vector<TargetSources>, SourceScanFailure> collectSources(const std::filesystem::path& projectRoot,
                                                                                          const std::vector<Target>& targets);

}  // namespace scrap::Project
