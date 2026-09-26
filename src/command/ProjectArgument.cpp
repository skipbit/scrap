#include "command/ProjectArgument.h"

#include "command/InvocationContext.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "project/ProjectLoader.h"

#include <filesystem>
#include <optional>
#include <ostream>
#include <utility>

namespace scrap::Command {

namespace {

/**
 * The path argument taken against the working directory, or the working
 * directory itself.
 */
std::filesystem::path startDirectory(const InvocationContext& ctx)
{
    if (ctx.options.positional.empty()) {
        return ctx.env->workingDirectory;
    }
    return ctx.env->workingDirectory / ctx.options.positional.front();
}

}  // anonymous namespace

std::optional<Project::LoadedProject> loadProjectAt(const InvocationContext& ctx, std::ostream& err)
{
    if ((! ctx.options.positional.empty()) && ctx.options.positional.front().empty()) {
        err << renderEmptyPathArgument();
        return std::nullopt;
    }

    auto project = Project::loadProject(startDirectory(ctx));
    if (! project.has_value()) {
        err << renderProjectError(project.error());
        return std::nullopt;
    }
    return std::move(*project);
}

}  // namespace scrap::Command
