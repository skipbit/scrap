#pragma once

#include "project/Manifest.h"
#include "project/SourceCollector.h"

#include <array>
#include <expected>
#include <filesystem>
#include <string_view>
#include <vector>

namespace scrap::Project {

/// Extensions a header is recognised by.
inline constexpr std::array<std::string_view, 4> HeaderExtensions{ ".h", ".hh", ".hpp", ".hxx" };

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
