#pragma once

#include "command/RuntimeEnvironment.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scrap::Command {

/**
 * @brief The compiler a build uses, and where it came from.
 */
struct ChosenCompiler {
    std::filesystem::path compiler;                                ///< The program to run, as an absolute path.
    std::vector<std::filesystem::path> runtimeLibraryDirectories;  ///< Where its runtime libraries are; none for the system's compiler.
    std::vector<std::string> compileArguments;                     ///< What it is given to compile; none for the system's compiler.
    std::vector<std::string> linkArguments;                        ///< What it is given to link; none for the system's compiler.
    bool installed;                                                ///< Whether it is the compiler of a toolchain scrap installed.
    std::string announcement;                                      ///< The line naming it, ending in a newline.
};

/**
 * @brief Choose the compiler a build of a project uses.
 *
 * The first of these that names a toolchain is used: SCRAP_TOOLCHAIN, the
 * project's [toolchain] name (@p projectToolchain), then the global
 * default. A specifier is matched against the installed toolchains alone,
 * as the newest one it names; one that is not installed is an error, never
 * installed in passing. system, or nothing named at all, finds the
 * compiler the system provides, reading CXX; an installed toolchain is
 * never chosen in its place.
 *
 * The global default is read only when nothing before it names a
 * toolchain, so a config.toml that cannot be read stops only a build that
 * would use it.
 *
 * @return The compiler, or the error and hint lines to report.
 */
[[nodiscard]] std::expected<ChosenCompiler, std::string> chooseCompiler(const RuntimeEnvironment& env, const std::optional<std::string>& projectToolchain);

}  // namespace scrap::Command
