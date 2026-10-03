#pragma once

#include "project/LanguageStandard.h"

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
 * @brief What a target adds to the commands that build it, as scrap.toml
 *        writes it.
 */
struct TargetSettings {
    std::vector<std::filesystem::path> includeDirectories;  ///< include-dirs, relative to the project root.
    std::vector<std::string> defines;                       ///< defines, each NAME or NAME=VALUE.
    std::vector<std::string> compileFlags;                  ///< compile-flags.
    std::vector<std::string> linkFlags;                     ///< link-flags.
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
    /**
     * The file src names, relative to the project root: an executable's entry
     * point, or a file a library adds; empty for a library that names none.
     */
    std::filesystem::path source;
    /// Settings for this target alone.
    TargetSettings settings{};
    /// Settings for a library and every target that uses it, from [lib.public].
    TargetSettings publicSettings{};
};

/**
 * @brief The [package] table of scrap.toml.
 */
struct Package {
    std::string name;
    std::string version;
    LanguageStandard standard = LanguageStandard::Cxx23;
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
