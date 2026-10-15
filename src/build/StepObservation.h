#pragma once

#include "build/BuildStep.h"
#include "build/StepRecord.h"

#include <filesystem>
#include <optional>
#include <set>
#include <vector>

namespace scrap::Build {

/**
 * @brief The stamp of @p path, named relative to @p directory, following
 *        links, or nothing when it cannot be examined.
 */
[[nodiscard]] std::optional<InputStamp> stampOf(const std::filesystem::path& path, const std::filesystem::path& directory);

/**
 * @brief The paths where a file could appear in place of one of @p files,
 *        whatever named it.
 *
 * A file below one of @p places is named there by its path below that place,
 * so the same name below each of the other places is listed, sorted, unless
 * it is that file itself. Paths are compared after lexical normalisation.
 * Whether anything is at a path is not looked at.
 */
[[nodiscard]] std::vector<std::filesystem::path> shadowingPaths(const std::set<std::filesystem::path>& places,
                                                                const std::vector<std::filesystem::path>& files);

/**
 * @brief The directory of @p path and each one above it, nearest first, up
 *        to "." for a relative path or the root for an absolute one.
 *
 * The path is lexically normal. The disk is not looked at.
 */
[[nodiscard]] std::vector<std::filesystem::path> directoriesAbove(const std::filesystem::path& path);

/**
 * @brief What @p step reads, as it stands now, in the form a record keeps.
 *
 * The record holds the command line, then a stamp for the program, for each
 * of the step's inputs and, for a compilation, for each file its dependency
 * file lists. Paths are relative to the step's directory, as the step names
 * them.
 *
 * It also holds where a header could appear in place of one the compilation
 * read. Each header the listed files of the project name is looked for as
 * Compile::headerSearch() orders it, and the paths that hold nothing, up to
 * the first that holds a file when that ends the search, are listed,
 * sorted. The files of the system are not read. When a file names a header
 * by a macro, the paths shadowingPaths() lists for the files read, with all
 * the places the compiler looks for this step, that hold nothing are listed
 * as well.
 *
 * @return The record, or nothing when whether the step needs to run cannot be
 *         told: an input, the program or a listed file cannot be examined, or
 *         a compilation has no dependency file, or one that cannot be read.
 */
[[nodiscard]] std::optional<StepRecord> observeStep(const BuildStep& step);

/**
 * @brief The directories to watch for a header that appears in place of one
 *        @p step read, relative to the step's directory, sorted.
 *
 * For every path observeStep() could look at, this is the nearest of
 * directoriesAbove() that is a directory now. If it was there when the step
 * started, a file appearing in it changes its time; if it was made while the
 * step ran, its own time is after the start. A file that appears while the
 * step runs is not among the absent paths of its record.
 *
 * @return The directories, none for a step that is not a compilation, or
 *         nothing when its dependency file cannot be read.
 */
[[nodiscard]] std::optional<std::vector<std::filesystem::path>> watchedDirectories(const BuildStep& step);

}  // namespace scrap::Build
