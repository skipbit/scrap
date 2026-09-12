#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace scrap::Project {

/**
 * @brief Kind of artifact a build target produces.
 */
enum class TargetKind : std::uint8_t {
    Executable,
    Library
};

/**
 * @brief A single build target and the source file it starts from.
 *
 * Declared by a [[bin]] or [[lib]] table in scrap.toml, or inferred from the
 * default project layout when the manifest declares none.
 */
struct Target {
    TargetKind kind = TargetKind::Executable;
    std::string name;
    std::filesystem::path entryPoint;  ///< Relative to the project root.
};

/**
 * @brief The [package] table of scrap.toml.
 */
struct Package {
    std::string name;
    std::string version;
    std::string standard;  ///< C++ language standard, e.g. "23".
};

/**
 * @brief The parsed contents of a scrap.toml file.
 *
 * Holds only what the manifest itself states. Filling in targets from the
 * default layout is the job of resolveTargets(), which needs to tell a
 * manifest that declared nothing from one that declared an empty list, so
 * declaresTargets records which of the two it was.
 */
struct Manifest {
    Package package;

    /**
     * Targets the manifest declares, in no particular order: executables come
     * before libraries whatever order they were written in. Build order is
     * derived from what targets depend on, never from this sequence.
     */
    std::vector<Target> targets;

    /**
     * Whether the manifest wrote a [[bin]] or [[lib]] declaration at all.
     * An empty declaration (bin = []) says the project builds nothing, which
     * is a different statement from declaring no targets.
     */
    bool declaresTargets = false;
};

}  // namespace scrap::Project
