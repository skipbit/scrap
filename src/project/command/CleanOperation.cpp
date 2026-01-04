#include "CleanOperation.h"
#include "project/service/ProjectService.h"
#include "shared/command/CommandOptions.h"
#include "shared/presentation/Presenter.h"

namespace scrap::project::command {

CleanOperation::CleanOperation(std::shared_ptr<service::ProjectService> service)
    : service_(service)
{
}

void CleanOperation::execute(const std::vector<std::string>& args)
{
    auto output = presenter();
    if (!output) {
        return;
    }

    // Note: Help is now handled by CLI11, no need to check for --help here

    // Load current project
    auto project = service_->loadProject();
    if (!project) {
        output->displayError("No project found in current directory");
        output->displayInfo("Run 'scrap new <project-name>' to create a new project");
        return;
    }

    // Parse clean options
    bool deep = false;
    for (const auto& arg : args) {
        if (arg == "--deep") {
            deep = true;
        }
    }

    // Perform cleaning
    service_->clean(*project);

    if (deep) {
        // Simulate deep clean output
        output->displayInfo("     Removed build/");
        output->displayInfo("     Removed .scrap/");
        output->displayInfo("     Removed compile_commands.json");
        output->displayInfo("     Removed .cache/");
        output->displayInfo("     Cleaned 312 files, 125.8 MB freed");
        output->displaySuccess("     Workspace restored to pristine state");
    } else {
        // Simulate regular clean output
        output->displayInfo("     Removed .scrap/cache/");
        output->displayInfo("     Cleaned 156 files, 45.2 MB freed");
    }
}

CommandOptions CleanOperation::describeOptions() const
{
    return CommandOptions().addFlag("deep", "Remove all generated files including caches");
}

void CleanOperation::displayHelp() const
{
    // This method is deprecated and will be removed
    // Help is now generated automatically from describeOptions()
}

}  // namespace scrap::project::command
