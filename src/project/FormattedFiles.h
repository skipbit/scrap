#pragma once

#include "project/DirectoryWalk.h"
#include "project/Manifest.h"

#include <expected>
#include <filesystem>
#include <vector>

namespace scrap::Project {

/**
 * @brief The files of a project that scrap format covers.
 *
 * These are the sources and headers below src/, the headers below include/
 * and below every directory a target names in include-dirs, its own or its
 * public ones, and every file a target names in src. Extensions are matched
 * in lower case, as collectSources() matches them. Nothing below build/ is
 * covered. A named file is covered whether or not it exists, so the tool
 * reports one that does not.
 *
 * Nothing a format would change outside the project or through a link is
 * covered: a directory to walk that resolves outside the project is not
 * walked, and a symbolic link to a file, which a format replaces with a copy,
 * is not covered wherever it sits.
 *
 * A directory that does not exist holds nothing. One that exists and cannot
 * be read is reported with the path that failed.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param targets Targets of the project, as resolveTargets() returned them.
 * @return Paths relative to the project root, normalized, sorted and each
 *         once, or the directory that could not be read.
 */
[[nodiscard]] std::expected<std::vector<std::filesystem::path>, SourceScanFailure> filesToFormat(const std::filesystem::path& projectRoot,
                                                                                                 const std::vector<Target>& targets);

}  // namespace scrap::Project
