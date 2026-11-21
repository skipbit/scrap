#pragma once

#include "compile/BuildProfile.h"
#include "compile/CompileCommand.h"
#include "compile/CompilePlanner.h"
#include "project/Manifest.h"
#include "project/ProjectLoader.h"
#include "project/SourceCollector.h"

#include <expected>  // IWYU pragma: keep
#include <vector>

namespace scrap::Command {

struct RuntimeEnvironment;

/**
 * @brief The commands that compile a project, and what they were made from.
 */
struct ProjectCompiles {
    Compile::BuildSettings settings;
    std::vector<Project::TargetSources> sources;
    std::vector<Compile::CompileCommand> commands;
};

/**
 * @brief The targets of a loaded project, reporting on standard error a
 *        project that declares none, or more than one library.
 *
 * @return The targets, or the exit code of the failure it reported (1).
 */
[[nodiscard]] std::expected<std::vector<Project::Target>, int> resolveProjectTargets(const Project::LoadedProject& project);

/**
 * @brief Writes @p commands to compile_commands.json in the directory of
 *        @p profile, reporting a failure on standard error.
 *
 * @return Nothing, or the exit code of the failure it reported (1).
 */
[[nodiscard]] std::expected<void, int> writeProjectDatabase(const Project::LoadedProject& project,
                                                            Compile::BuildProfile profile,
                                                            const std::vector<Compile::CompileCommand>& commands);

/**
 * @brief The commands that compile every source of @p targets for
 *        @p profile, as scrap build runs them, without compiling.
 *
 * Collects the sources, resolves the compilers, named on standard error
 * with where they came from, writes the commands to compile_commands.json
 * in the directory of @p profile, and then checks that the compilers can
 * build the standards the project states, so an editor reads the commands
 * whether or not they can run. Each failure is reported on standard error
 * with the next step.
 *
 * @param env The environment the command runs in.
 * @param project The project to compile.
 * @param profile What to compile it for.
 * @param targets The targets of the project; at least one.
 * @return The commands, or the exit code of the failure it reported (1).
 */
[[nodiscard]] std::expected<ProjectCompiles, int> planProjectCompiles(const RuntimeEnvironment& env,
                                                                      const Project::LoadedProject& project,
                                                                      Compile::BuildProfile profile,
                                                                      const std::vector<Project::Target>& targets);

}  // namespace scrap::Command
