#pragma once

#include "build/BuildStep.h"
#include "build/StepRecord.h"
#include "compile/IncludeDirectives.h"

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
 * @brief The paths where the compiler looks for @p included, named by a file
 *        in @p directory, in the order it looks, each once.
 *
 * A quoted name is looked for in @p directory, then below each of
 * @p searched; a bracketed one only below @p searched. A name the file
 * names with include_next is looked for everywhere a quoted one is, since
 * where the file itself was found is not known. Paths are lexically
 * normalised.
 */
[[nodiscard]] std::vector<std::filesystem::path> searchPaths(const Compile::IncludedName& included,
                                                             const std::filesystem::path& directory,
                                                             const std::vector<std::filesystem::path>& searched);

/**
 * @brief The directories whose entries change when a file appears at one of
 *        @p paths: the directory of each path and each one above it, up to
 *        the outermost of @p places above it, sorted.
 */
[[nodiscard]] std::vector<std::filesystem::path> shadowingDirectories(const std::set<std::filesystem::path>& places,
                                                                      const std::vector<std::filesystem::path>& paths);

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
 * searchPaths() orders it, and the paths that hold nothing, up to the first
 * that holds a file, are listed, sorted. The files of the system are not
 * read. When a file names a header by a macro, the paths are instead those
 * shadowingPaths() lists for the files read, with all the places the
 * compiler looks for this step, that hold nothing.
 *
 * @return The record, or nothing when whether the step needs to run cannot be
 *         told: an input, the program or a listed file cannot be examined, or
 *         a compilation has no dependency file, or one that cannot be read.
 */
[[nodiscard]] std::optional<StepRecord> observeStep(const BuildStep& step);

/**
 * @brief The directories where a header could appear in place of one
 *        @p step read, as shadowingDirectories() names them for every path
 *        observeStep() could look at, relative to the step's directory.
 *
 * A file that appears while the step runs is not among the absent paths of
 * its record, but changes one of these directories.
 *
 * @return The directories, none for a step that is not a compilation, or
 *         nothing when its dependency file cannot be read.
 */
[[nodiscard]] std::optional<std::vector<std::filesystem::path>> watchedDirectories(const BuildStep& step);

}  // namespace scrap::Build
