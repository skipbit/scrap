#pragma once

#include "project/Manifest.h"

#include <array>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace scrap::Project {

/// Extensions a source file is recognised by.
inline constexpr std::array<std::string_view, 3> SourceExtensions{ ".cpp", ".cc", ".cxx" };

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
 * @brief A declared file is a source below src/ reached under another path.
 *
 * A spelling that differs only in case, on a file system that ignores case,
 * and a hard link both reach the one file under two paths, which a build
 * would compile twice.
 */
struct SourceNamedTwice {
    std::filesystem::path declared;  ///< As the manifest names it, normalized.
    std::filesystem::path found;     ///< As the scan found it, relative to the project root.
};

/**
 * @brief Why the sources of a project could not be decided.
 */
using SourceCollectionError = std::variant<SourceScanFailure, SourceNamedTwice>;

/**
 * @brief Why an entry below src/ that looks like a source is not built.
 */
enum class LeftOutReason : std::uint8_t {
    /// Its extension is a source extension spelled in another case.
    ExtensionSpelling,
    /// It is a symbolic link to a directory, which the scan does not follow.
    DirectoryLink,
    /// It is a symbolic link to a file that does not exist.
    BrokenLink,
};

/**
 * @brief An entry below src/ that the build leaves out.
 */
struct LeftOutEntry {
    std::filesystem::path path;  ///< Relative to the project root.
    LeftOutReason reason;

    friend bool operator==(const LeftOutEntry&, const LeftOutEntry&) = default;
};

/**
 * @brief The sources of each target, and what below src/ is left out.
 */
struct CollectedSources {
    std::vector<TargetSources> targets;  ///< In the order the targets were given.
    std::vector<LeftOutEntry> leftOut;   ///< Sorted by path.
};

/**
 * @brief Decide which sources each target is built from.
 *
 * Without a library, every source below src/ belongs to each executable,
 * except the entry points of the other executables: two executables share the
 * code beside them and differ in the file that starts each one. A target keeps
 * its own entry point wherever it sits, since a declaration states what to
 * build whether or not the default layout expects the file there. Paths are
 * compared once normalized, so "./src/main.cpp" and "src/main.cpp" name the
 * same file. A declared file that the file system reports as the same file
 * as a source below src/ under another path is reported with both paths.
 *
 * A source is recognised by its extension, spelled in lower case: .cpp, .cc
 * or .cxx. The list comes back sorted, so a build reads the same sources
 * whatever order the file system reports its entries in.
 *
 * A project without src/ is not a failure, since a target can declare an
 * entry point anywhere. A directory that exists and cannot be read, or whose
 * state cannot be determined, is reported with the path that failed. A link
 * to a file is read as the file it names.
 *
 * What the scan does not build is left out and listed, so the build goes on
 * as before and can still name it: a file whose extension is a source
 * extension in another case, a symbolic link to a directory, which is not
 * followed, and a link named as a source that names no file. A declared file
 * is built and not listed.
 *
 * A project with a library gives the sources below src/ to the library, except
 * the entry points of the executables, and adds the file the library names,
 * if any. Each executable is then built from its entry point alone and links
 * the library.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param targets Targets to build, as resolveTargets() returned them.
 * @return Each target with its sources and what was left out, or the
 *         directory that could not be read or the declared file that names a
 *         source under another path.
 */
[[nodiscard]] std::expected<CollectedSources, SourceCollectionError> collectSources(const std::filesystem::path& projectRoot,
                                                                                    const std::vector<Target>& targets);

}  // namespace scrap::Project
