#include "command/NewCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/PrintableText.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "project/DefaultTemplate.h"
#include "project/ProjectCreator.h"
#include "project/ProjectFileSystem.h"

#include <iostream>
#include <string>

namespace scrap::Command {

NewCommandHandler::NewCommandHandler(Project::ProjectFileSystem& fileSystem)
    : _fileSystem(&fileSystem)
{
}

auto NewCommandHandler::execute(const InvocationContext& ctx) -> int
{
    // The parser requires the name, so the first positional is there. Reading
    // an empty list as an empty name keeps a caller that skips the parser from
    // reading past the end.
    const std::string name = ctx.options.positional.empty() ? std::string{} : ctx.options.positional.front();

    const auto root = Project::createProject(*_fileSystem, ctx.env->workingDirectory, name, Project::defaultTemplateFiles);
    if (! root.has_value()) {
        std::cerr << renderCreateProjectError(root.error());
        return 1;
    }

    // What the command did, not what it returns: the line goes where the
    // progress and the failures of every command go.
    std::cerr << "Created project '" << name << "' at '" << printablePath(*root) << "'\n";
    return 0;
}

}  // namespace scrap::Command
