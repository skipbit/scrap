#include "RunOperation.h"
#include "project/service/ProjectService.h"
#include "project/model/Project.h"
#include "shared/presentation/Presenter.h"
#include "shared/command/CommandOptions.h"
#include <sstream>

namespace scrap::project::command {

RunOperation::RunOperation(std::shared_ptr<service::ProjectService> service)
    : service_(service)
{
}

void RunOperation::execute(const std::vector<std::string>& args)
{
    auto output = presenter();
    if (!output) {
        return;
    }

    // Note: Help is now handled by CLI11, no need to check for --help here

    try {
        // Load current project
        auto project = service_->loadProject();
        if (!project) {
            output->displayError("No project found in current directory");
            output->displayInfo("Run 'scrap new <project-name>' to create a new project");
            return;
        }

        if (!project->isApplication()) {
            output->displayError("Cannot run library project");
            output->displayInfo("Libraries cannot be executed directly");
            return;
        }

        // Parse run options
        auto options = model::RunOptions::parse(args);

        // Check if build is needed (always build in mock implementation)
        auto buildOptions = model::BuildOptions();
        buildOptions.mode = model::BuildMode::Debug;

        std::stringstream ss;
        ss << "   Compiling " << project->name().toString()
           << " v" << project->version().toString();
        if (project->path()) {
            ss << " (" << project->path()->string() << ")";
        }
        output->displayInfo(ss.str());

        // Build the project first
        auto buildResult = service_->build(*project, buildOptions);
        if (!buildResult.isSuccess()) {
            output->displayError("Build failed, cannot run");
            return;
        }

        // Display build completion
        ss.str("");
        ss << "    Finished dev [unoptimized + debuginfo] target(s) in "
           << std::fixed << std::setprecision(2)
           << buildResult.duration.count() / 1000.0 << "s";
        output->displayInfo(ss.str());

        // Display run command
        ss.str("");
        ss << "     Running `" << project->name().toString();
        for (const auto& arg : options.arguments) {
            ss << " " << arg;
        }
        ss << "`";
        output->displayInfo(ss.str());

        // Execute the project
        service_->run(*project, options);

    } catch (const std::exception& e) {
        output->displayError(std::string("Run failed: ") + e.what());
    }
}

CommandOptions RunOperation::describeOptions() const
{
    return CommandOptions()
        .addOption(CommandOption("working-dir", "Set working directory", OptionType::String));
    // Note: Arguments after -- are handled specially by CLI11's allow_extras()
}

void RunOperation::displayHelp() const
{
    // This method is deprecated and will be removed
    // Help is now generated automatically from describeOptions()
}

} // namespace scrap::project::command
