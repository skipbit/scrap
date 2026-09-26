#pragma once

#include "project/ProjectLoader.h"

#include <iosfwd>
#include <optional>

namespace scrap::Command {

struct InvocationContext;

/**
 * @brief Load the project a command's optional path argument belongs to.
 *
 * The search starts at the path taken against the working directory, or at
 * the working directory when no path is given. An explicitly empty path is
 * usually an unset variable, so it is reported instead of standing for the
 * working directory.
 *
 * @param ctx Invocation context. Its first positional is the optional path.
 * @param err Where the reason goes when there is no project.
 * @return The project, or nothing once the reason has been written to @p err.
 */
[[nodiscard]] std::optional<Project::LoadedProject> loadProjectAt(const InvocationContext& ctx, std::ostream& err);

}  // namespace scrap::Command
