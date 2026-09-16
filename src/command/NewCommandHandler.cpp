#include "command/NewCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "project/DefaultTemplate.h"
#include "project/ProjectCreator.h"
#include "project/ProjectFileSystem.h"

#include <iostream>
#include <string>

namespace scrap::Command {

NewCommandHandler::NewCommandHandler(Project::ProjectFileSystem& fileSystem)
    : fileSystem_(&fileSystem)
{
}

auto NewCommandHandler::execute(const InvocationContext& ctx) -> int
{
    // The parser requires the name, so the first positional is there. Reading
    // an empty list as an empty name keeps a caller that skips the parser from
    // reading past the end.
    const std::string name = ctx.options.positional.empty() ? std::string{} : ctx.options.positional.front();

    const auto root =
        Project::createProject(*fileSystem_, ctx.env->workingDirectory, name, Project::defaultTemplateFiles);
    if (! root.has_value()) {
        std::cerr << renderCreateProjectError(root.error());
        return 1;
    }

    std::cout << "Created project '" << name << "' at '" << printablePath(*root) << "'\n";
    return 0;
}

}  // namespace scrap::Command
