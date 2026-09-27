#include "command/ProjectArgument.h"

#include "command/InvocationContext.h"
#include "command/RuntimeEnvironment.h"
#include "project/ProjectLoader.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
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

std::expected<Project::LoadedProject, ProjectArgumentError> loadProjectAt(const InvocationContext& ctx)
{
    if ((! ctx.options.positional.empty()) && ctx.options.positional.front().empty()) {
        return std::unexpected{ ProjectArgumentError{ EmptyPathArgument{} } };
    }

    auto project = Project::loadProject(startDirectory(ctx));
    if (! project.has_value()) {
        return std::unexpected{ ProjectArgumentError{ project.error() } };
    }
    return std::move(*project);
}

}  // namespace scrap::Command
