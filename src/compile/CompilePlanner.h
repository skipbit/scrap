#pragma once

#include "compile/ArchiveCommand.h"
#include "compile/BuildProfile.h"
#include "compile/CompileCommand.h"
#include "compile/CompilerDriver.h"
#include "compile/LinkCommand.h"
#include "project/LanguageStandard.h"
#include "project/SourceCollector.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace scrap::Compile {

/**
 * @brief What every command of one build shares.
 */
struct BuildSettings {
    std::filesystem::path projectRoot;     ///< Where the commands run: the project root, absolute.
    std::filesystem::path buildDirectory;  ///< Where they write, relative to the project root.
    std::filesystem::path compiler;        ///< The compiler to run, absolute.
    CompilerDriver driver;                 ///< How that compiler takes its options.
    Project::LanguageStandard standard;    ///< The standard the manifest states.
    BuildProfile profile;                  ///< What the compiler is asked to build for.
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
 * builds for the profile with the common warnings on, keeps colour in the
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
[[nodiscard]] std::vector<CompileCommand> planCompileCommands(const BuildSettings& settings,
                                                              const std::vector<Project::TargetSources>& targets);

/**
 * @brief Where the executable of a target is written.
 *
 * @param buildDirectory The build directory, as BuildSettings holds it.
 * @param target The name of the executable target.
 * @return bin/<target> in the build directory.
 */
[[nodiscard]] std::filesystem::path executableFile(const std::filesystem::path& buildDirectory, std::string_view target);

/**
 * @brief Where the static library of a target is written.
 *
 * @param buildDirectory The build directory, as BuildSettings holds it.
 * @param target The name of the library target.
 * @return lib/lib<target>.a in the build directory.
 */
[[nodiscard]] std::filesystem::path libraryFile(const std::filesystem::path& buildDirectory, std::string_view target);

/**
 * @brief Decide the command that archives each library.
 *
 * A library is written to lib/lib<target>.a in the build directory, from the
 * object files planCompileCommands() gives its sources, in the same order.
 *
 * @param settings What the commands of the build share.
 * @param archiver The archiver to run, as findArchiver() found it.
 * @param targets Each target with its sources, as collectSources() returned them.
 * @return One command per library, in the order the targets were given.
 */
[[nodiscard]] std::vector<ArchiveCommand> planArchiveCommands(const BuildSettings& settings,
                                                              const std::filesystem::path& archiver,
                                                              const std::vector<Project::TargetSources>& targets);

/**
 * @brief Decide the command that links each executable.
 *
 * An executable is written to bin/<target> in the build directory, from the
 * object files planCompileCommands() gives its sources, in the same order,
 * followed by the library among @p targets, if any: an executable uses the
 * library of its project without naming it.
 *
 * @param settings What the commands of the build share.
 * @param targets Each target with its sources, as collectSources() returned them.
 * @return One command per executable, in the order the targets were given.
 */
[[nodiscard]] std::vector<LinkCommand> planLinkCommands(const BuildSettings& settings,
                                                        const std::vector<Project::TargetSources>& targets);

}  // namespace scrap::Compile
