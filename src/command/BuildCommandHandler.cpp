#include "command/BuildCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "project/ProjectLoader.h"
#include "project/TargetResolver.h"

#include <filesystem>
#include <iostream>

namespace scrap::Command {

namespace {

/**
 * The path argument taken against the working directory, or the working
 * directory itself.
 */
auto startDirectory(const InvocationContext& ctx) -> std::filesystem::path
{
    if (ctx.options.positional.empty()) {
        return ctx.env->workingDirectory;
    }
    return ctx.env->workingDirectory / ctx.options.positional.front();
}

}  // anonymous namespace

auto BuildCommandHandler::execute(const InvocationContext& ctx) -> int
{
    // An explicitly empty argument is usually an unset variable, so it is
    // reported as an error instead of standing for the working directory.
    if (! ctx.options.positional.empty() && ctx.options.positional.front().empty()) {
        std::cerr << renderEmptyPathArgument();
        return 1;
    }

    const auto project = Project::loadProject(startDirectory(ctx));
    if (! project.has_value()) {
        std::cerr << renderProjectError(project.error());
        return 1;
    }

    // An empty declaration states that the project builds nothing, which is a
    // different answer from finding nothing where no declaration was written.
    const auto targets = Project::resolveTargets(project->root, project->manifest);
    if (targets.empty() && ! project->manifest.declaresTargets) {
        std::cerr << renderNoTargetToBuild(project->root);
        return 1;
    }

    // Placeholder output until the build compiles the project.
    std::cout << "build: not yet implemented\n";
    return 0;
}

}  // namespace scrap::Command
