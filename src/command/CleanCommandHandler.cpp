#include "command/CleanCommandHandler.h"

#include "build/BuildOutput.h"
#include "command/PrintableText.h"
#include "command/ProjectArgument.h"
#include "command/ProjectDiagnostic.h"

#include <filesystem>
#include <iostream>

namespace scrap::Command {

int CleanCommandHandler::execute(const InvocationContext& ctx)
{
    const auto project = loadProjectAt(ctx, std::cerr);
    if (! project.has_value()) {
        return 1;
    }

    const std::filesystem::path directory = project->root / Build::OutputDirectory;
    const auto removed = Build::removeOutput(directory);
    if (! removed.has_value()) {
        std::cerr << renderOutputRemovalFailure(removed.error());
        return 1;
    }

    if (*removed == Build::OutputRemoval::NothingThere) {
        std::cerr << "Nothing to remove at '" << printablePath(directory) << "'\n";
    } else {
        std::cerr << "Removed '" << printablePath(directory) << "'\n";
    }
    return 0;
}

}  // namespace scrap::Command
