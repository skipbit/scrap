#include "RunOperation.h"
#include "project/service/ProjectService.h"
#include "project/model/Project.h"
#include "shared/presentation/Presenter.h"
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

    // Check for help (but not after -- separator)
    bool foundSeparator = false;
    for (const auto& arg : args) {
        if (arg == "--") {
            foundSeparator = true;
            break;
        }
        if ((arg == "--help" || arg == "-h") && !foundSeparator) {
            displayHelp();
            return;
        }
    }

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

void RunOperation::displayHelp() const
{
    auto output = presenter();
    if (!output) {
        return;
    }

    output->displayInfo("Run the current project executable");
    output->displayInfo("");
    output->displayInfo("Usage: scrap run [options] [-- <args>...]");
    output->displayInfo("");
    output->displayInfo("Options:");
    output->displayInfo("  --working-dir=<path>  Set working directory");
    output->displayInfo("");
    output->displayInfo("Arguments:");
    output->displayInfo("  --                    Pass remaining arguments to the executable");
    output->displayInfo("  <args>...             Arguments to pass to the executable");
    output->displayInfo("");
    output->displayInfo("Examples:");
    output->displayInfo("  scrap run                          # Run without arguments");
    output->displayInfo("  scrap run -- --help                # Pass --help to executable");
    output->displayInfo("  scrap run -- input.txt output.txt  # Pass file arguments");
    output->displayInfo("  scrap run --working-dir=/tmp       # Run in different directory");
    output->displayInfo("");
    output->displayInfo("Note: This command will build the project if needed.");
}

} // namespace scrap::project::command
