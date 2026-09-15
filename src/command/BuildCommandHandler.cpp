#include "command/BuildCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "project/ProjectLoader.h"

#include <filesystem>
#include <iostream>

namespace scrap::Command {

namespace {

/**
 * The path argument taken against the working directory, or the working
 * directory itself. An omitted positional arrives as an empty string.
 */
auto startDirectory(const InvocationContext& ctx) -> std::filesystem::path
{
    if (ctx.options.positional.empty() || ctx.options.positional.front().empty()) {
        return ctx.env->workingDirectory;
    }
    return ctx.env->workingDirectory / ctx.options.positional.front();
}

}  // anonymous namespace

auto BuildCommandHandler::execute(const InvocationContext& ctx) -> int
{
    const auto project = Project::loadProject(startDirectory(ctx));
    if (! project.has_value()) {
        std::cerr << renderProjectError(project.error());
        return 1;
    }

    // Placeholder output until the build compiles the project.
    std::cout << "build: not yet implemented\n";
    return 0;
}

}  // namespace scrap::Command
