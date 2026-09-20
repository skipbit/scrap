#pragma once

#include "compile/CompileCommand.h"
#include "compile/CompilerDriver.h"
#include "compile/LinkCommand.h"
#include "project/LanguageStandard.h"
#include "project/SourceCollector.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace scrap::Compile {

/// Directory, relative to the project root, a debug build writes to.
inline constexpr std::string_view DebugBuildDirectory = "build/debug";

/**
 * @brief What every command of one build shares.
 */
struct BuildSettings {
    std::filesystem::path projectRoot;     ///< Where the commands run: the project root, absolute.
    std::filesystem::path buildDirectory;  ///< Where they write, relative to the project root.
    std::filesystem::path compiler;        ///< The compiler to run, absolute.
    CompilerDriver driver;                 ///< How that compiler takes its options.
    Project::LanguageStandard standard;    ///< The standard the manifest states.
};

/**
 * @brief Decide the command that compiles each source of each target.
 *
 * Each command runs in the project root and names the source and the object
 * file relative to it, so a diagnostic reads "src/main.cpp:3:5". The object
 * file mirrors the source below obj/<target>/ in the build directory and
 * keeps its extension: a source shared by two targets is compiled once for
 * each, and src/a/x.cpp, src/b/x.cpp and src/x.cc stay apart.
 *
 * A command selects the standard as the driver spells it for its compiler,
 * or by the standard's own name when that compiler cannot build it, so the
 * compilation database still names the standard the manifest states. It
 * builds for debugging with the common warnings on, keeps colour in the
 * compiler's diagnostics where the driver knows how, and searches include/
 * for headers; the compiler passes over that directory when a project has
 * none. A source whose path starts with '-' or '@' is written as ./<path>,
 * so the compiler reads it as a file rather than as an option or as a file
 * of options.
 *
 * @param settings What the commands of the build share.
 * @param targets Each target with its sources, as collectSources() returned them.
 * @return One command per source of each target, targets in the order given.
 */
[[nodiscard]] auto
planCompileCommands(const BuildSettings& settings,
                    const std::vector<Project::TargetSources>& targets) -> std::vector<CompileCommand>;

/**
 * @brief Decide the command that links each executable.
 *
 * An executable is written to bin/<target> in the build directory, from the
 * object files planCompileCommands() gives its sources, in the same order.
 * Libraries are not linked.
 *
 * @param settings What the commands of the build share.
 * @param targets Each target with its sources, as collectSources() returned them.
 * @return One command per executable, in the order the targets were given.
 */
[[nodiscard]] auto planLinkCommands(const BuildSettings& settings,
                                    const std::vector<Project::TargetSources>& targets) -> std::vector<LinkCommand>;

}  // namespace scrap::Compile
