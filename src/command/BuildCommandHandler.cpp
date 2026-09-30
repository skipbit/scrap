#include "command/BuildCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/ProjectArgument.h"
#include "command/ProjectBuild.h"
#include "command/ProjectDiagnostic.h"

#include <iostream>

namespace scrap::Command {

int BuildCommandHandler::execute(const InvocationContext& ctx)
{
    const auto project = loadProjectAt(ctx);
    if (! project.has_value()) {
        std::cerr << renderProjectArgumentError(project.error());
        return 1;
    }
    const auto built = buildProject(*ctx.env, *project, requestedProfile(ctx.options));
    return built.has_value() ? 0 : built.error();
}

}  // namespace scrap::Command
