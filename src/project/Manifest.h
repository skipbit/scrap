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
 * Holds only what the manifest itself states. Targets are empty when the
 * manifest declares neither [[bin]] nor [[lib]]; filling them in from the
 * default layout is the job of resolveTargets().
 */
struct Manifest {
    Package package;
    std::vector<Target> targets;
};

}  // namespace scrap::Project
