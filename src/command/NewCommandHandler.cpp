#include "command/NewCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "project/DefaultTemplate.h"
#include "project/ProjectCreator.h"

#include <iostream>
#include <string>

namespace scrap::Command {

auto NewCommandHandler::execute(const InvocationContext& ctx) -> int
{
    const std::string name = ctx.options.positional.empty() ? std::string{} : ctx.options.positional.front();

    const auto root = Project::createProject(ctx.env->workingDirectory, name, Project::defaultTemplateFiles(name));
    if (! root.has_value()) {
        std::cerr << renderCreateProjectError(root.error());
        return 1;
    }

    std::cout << "Created project '" << name << "' at '" << root->string() << "'\n";
    return 0;
}

}  // namespace scrap::Command
