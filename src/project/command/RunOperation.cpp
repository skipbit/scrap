#include "RunOperation.h"
#include "project/service/ProjectService.h"
#include "project/model/Project.h"
#include "shared/presentation/Presenter.h"
#include <sstream>

namespace scrap::project::command {

RunOperation::RunOperation(std::shared_ptr<service::ProjectService> service)
    : service_(service) {
}

void RunOperation::execute(const std::vector<std::string>& args) {
    auto presenter = getPresenter();
    if (!presenter) {
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
            presenter->displayError("No project found in current directory");
            presenter->displayInfo("Run 'scrap new <project-name>' to create a new project");
            return;
        }

        if (!project->isApplication()) {
            presenter->displayError("Cannot run library project");
            presenter->displayInfo("Libraries cannot be executed directly");
            return;
        }

        // Parse run options
        auto options = model::RunOptions::parse(args);

        // Check if build is needed (always build in mock implementation)
        auto buildOptions = model::BuildOptions();
        buildOptions.mode = model::BuildMode::Debug;

        std::stringstream ss;
        ss << "   Compiling " << project->getName().toString()
           << " v" << project->getVersion().toString();
        if (project->getPath()) {
            ss << " (" << project->getPath()->string() << ")";
        }
        presenter->displayInfo(ss.str());

        // Build the project first
        auto buildResult = service_->build(*project, buildOptions);
        if (!buildResult.isSuccess()) {
            presenter->displayError("Build failed, cannot run");
            return;
        }

        // Display build completion
        ss.str("");
        ss << "    Finished dev [unoptimized + debuginfo] target(s) in "
           << std::fixed << std::setprecision(2)
           << buildResult.duration.count() / 1000.0 << "s";
        presenter->displayInfo(ss.str());

        // Display run command
        ss.str("");
        ss << "     Running `" << project->getName().toString();
        for (const auto& arg : options.arguments) {
            ss << " " << arg;
        }
        ss << "`";
        presenter->displayInfo(ss.str());

        // Execute the project
        service_->run(*project, options);

    } catch (const std::exception& e) {
        presenter->displayError(std::string("Run failed: ") + e.what());
    }
}

void RunOperation::displayHelp() const {
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    presenter->displayInfo("Run the current project executable");
    presenter->displayInfo("");
    presenter->displayInfo("Usage: scrap run [options] [-- <args>...]");
    presenter->displayInfo("");
    presenter->displayInfo("Options:");
    presenter->displayInfo("  --working-dir=<path>  Set working directory");
    presenter->displayInfo("");
    presenter->displayInfo("Arguments:");
    presenter->displayInfo("  --                    Pass remaining arguments to the executable");
    presenter->displayInfo("  <args>...             Arguments to pass to the executable");
    presenter->displayInfo("");
    presenter->displayInfo("Examples:");
    presenter->displayInfo("  scrap run                          # Run without arguments");
    presenter->displayInfo("  scrap run -- --help                # Pass --help to executable");
    presenter->displayInfo("  scrap run -- input.txt output.txt  # Pass file arguments");
    presenter->displayInfo("  scrap run --working-dir=/tmp       # Run in different directory");
    presenter->displayInfo("");
    presenter->displayInfo("Note: This command will build the project if needed.");
}

} // namespace scrap::project::command
