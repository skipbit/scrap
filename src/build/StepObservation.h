#pragma once

#include "build/BuildStep.h"
#include "build/StepRecord.h"

#include <filesystem>
#include <optional>
#include <set>
#include <vector>

namespace scrap::Build {

/**
 * @brief The paths where a file could appear in place of one of @p files.
 *
 * A file below one of @p places is named there by its path below that place,
 * so the same name below each of the other places is listed, sorted, unless
 * it is that file itself. Paths are compared after lexical normalisation.
 * Whether anything is at a path is not looked at.
 */
[[nodiscard]] std::vector<std::filesystem::path> shadowingPaths(const std::set<std::filesystem::path>& places,
                                                                const std::vector<std::filesystem::path>& files);

/**
 * @brief What @p step reads, as it stands now, in the form a record keeps.
 *
 * The record holds the command line, then a stamp for the program, for each
 * of the step's inputs and, for a compilation, for each file its dependency
 * file lists. Paths are relative to the step's directory, as the step names
 * them.
 *
 * It also holds where a header could appear in place of one the compilation
 * read. A header read from below one of the places the compiler looks (the
 * directory of the source, a directory named for headers, or the directory
 * of a header read) is named there by its path below that place, so the same
 * name below each of the other places that holds nothing is listed, sorted.
 * Any place that holds a file of that name already is not listed, since the
 * file there was not read, and the places the system searches are not
 * looked at.
 *
 * @return The record, or nothing when whether the step needs to run cannot be
 *         told: an input, the program or a listed file cannot be examined, or
 *         a compilation has no dependency file, or one that cannot be read.
 */
[[nodiscard]] std::optional<StepRecord> observeStep(const BuildStep& step);

}  // namespace scrap::Build
