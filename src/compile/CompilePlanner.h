#pragma once

#include "compile/CompileCommand.h"
#include "project/Manifest.h"
#include "project/SourceCollector.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace scrap::Compile {

/// Directory, relative to the project root, a debug build writes to.
inline constexpr std::string_view DebugBuildDirectory = "build/debug";

/**
 * @brief Decide the command that compiles each source of each target.
 *
 * Each command runs in the project root and names the source and the object
 * file relative to it, so a diagnostic reads "src/main.cpp:3:5". The object
 * file mirrors the source below obj/<target>/ in the build directory and
 * keeps its extension: a source shared by two targets is compiled once for
 * each, and src/a/x.cpp, src/b/x.cpp and src/x.cc stay apart.
 *
 * The command carries the language standard the manifest states, spelled
 * -std=c++<std>, and include/ as a directory to search for headers. The
 * compiler passes over that directory when a project has none. A source whose
 * path starts with '-' is written as ./<path>, so the compiler reads it as a
 * file rather than as an option.
 *
 * @param projectRoot Directory the manifest was read from, absolute.
 * @param buildDirectory Directory the build writes to, relative to the project root.
 * @param package The manifest's [package] table.
 * @param targets Each target with its sources, as collectSources() returned them.
 * @param compiler The compiler to run, absolute.
 * @return One command per source of each target, targets in the order given.
 */
[[nodiscard]] auto planCompileCommands(const std::filesystem::path& projectRoot,
                                       const std::filesystem::path& buildDirectory,
                                       const Project::Package& package,
                                       const std::vector<Project::TargetSources>& targets,
                                       const std::filesystem::path& compiler) -> std::vector<CompileCommand>;

}  // namespace scrap::Compile
