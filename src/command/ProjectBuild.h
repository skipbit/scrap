#pragma once

#include "compile/BuildProfile.h"
#include "project/ProjectLoader.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string_view>

namespace scrap::Command {

struct ParsedOptions;
struct RuntimeEnvironment;

/// The option that asks a build, or the build before a run, for the release profile.
inline constexpr std::string_view ReleaseOption = "release";

/**
 * @brief The profile the options of a command ask for: release when
 *        ReleaseOption is given, debug otherwise.
 */
[[nodiscard]] Compile::BuildProfile requestedProfile(const ParsedOptions& options);

/**
 * @brief Build a loaded project, reporting each step and any failure on
 *        standard error.
 *
 * Decides what the project builds, writes the command for each source to
 * compile_commands.json in the directory of @p profile, then compiles and
 * links.
 * When nothing was found to build, its sources cannot be read, no compiler
 * can be used, or a step fails, reports the cause and the next step.
 *
 * @param env The environment the command runs in.
 * @param project The project to build.
 * @param profile What to build it for.
 * @return Where it built, relative to the project root, or the exit code of
 *   the failure it reported (1).
 */
[[nodiscard]] std::expected<std::filesystem::path, int> buildProject(const RuntimeEnvironment& env,
                                                                     const Project::LoadedProject& project,
                                                                     Compile::BuildProfile profile);

}  // namespace scrap::Command
