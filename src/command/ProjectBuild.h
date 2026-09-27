#pragma once

#include "project/ProjectLoader.h"

namespace scrap::Command {

struct RuntimeEnvironment;

/**
 * @brief Build a loaded project, reporting each step and any failure on
 *        standard error.
 *
 * Decides what the project builds, writes the command for each source to
 * compile_commands.json in the build directory, then compiles and links.
 * When nothing was found to build, its sources cannot be read, no compiler
 * can be used, or a step fails, reports the cause and the next step.
 *
 * @param env The environment the command runs in.
 * @param project The project to build.
 * @return 0 on success, 1 when it reports a failure.
 */
[[nodiscard]] int buildProject(const RuntimeEnvironment& env, const Project::LoadedProject& project);

}  // namespace scrap::Command
