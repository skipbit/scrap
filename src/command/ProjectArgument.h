#pragma once

#include "project/ProjectLoader.h"

#include <expected>  // IWYU pragma: keep
#include <variant>

namespace scrap::Command {

struct InvocationContext;

/**
 * @brief An explicitly empty path argument.
 */
struct EmptyPathArgument { };

/**
 * @brief Why the project of a path argument could not be loaded.
 */
using ProjectArgumentError = std::variant<EmptyPathArgument, Project::ProjectError>;

/**
 * @brief Load the project a command's optional path argument belongs to.
 *
 * The search starts at the path taken against the working directory, or at
 * the working directory when no path is given. An explicitly empty path is
 * usually an unset variable, so it is reported instead of standing for the
 * working directory.
 *
 * @param ctx Invocation context. Its first positional is the optional path.
 * @return The project, or why it could not be loaded.
 */
[[nodiscard]] std::expected<Project::LoadedProject, ProjectArgumentError> loadProjectAt(const InvocationContext& ctx);

}  // namespace scrap::Command
