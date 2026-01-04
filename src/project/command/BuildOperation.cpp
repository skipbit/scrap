#include "BuildOperation.h"
#include "project/service/ProjectService.h"
#include "project/model/Project.h"
#include "shared/presentation/Presenter.h"
#include "shared/command/CommandOptions.h"
#include <sstream>
#include <chrono>
#include <thread>

namespace scrap::project::command {

// Namespace alias for cleaner code
namespace Model = scrap::Project::Model;

BuildOperation::BuildOperation(std::shared_ptr<service::ProjectService> service)
    : service_(service)
{
}

void BuildOperation::execute(const std::vector<std::string>& args)
{
    auto output = presenter();
    if (!output) {
        return;
    }

    try {
        // Load current project
        auto project = service_->loadProject();
        if (!project) {
            output->displayError("No project found in current directory");
            output->displayInfo("Run 'scrap new <project-name>' to create a new project");
            return;
        }

        // Parse build options
        auto options = Model::BuildOptions::parse(args);

        // Display build start (cargo-style)
        if (options.clean) {
            output->displayInfo("   Cleaning previous build...");
            service_->clean(*project);
        }

        // Display resolving dependencies
        if (!project->dependencies().empty()) {
            output->displayInfo("   Resolving dependencies...");
            for (const auto& dep : project->dependencies()) {
                output->displaySuccess("     ✓ " + dep.name() + " " + dep.version() + " (cached)");
            }
        }

        // Start build process
        std::stringstream ss;
        ss << "   Compiling " << project->name().toString()
           << " v" << project->version().toString();
        if (project->path()) {
            ss << " (" << project->path()->string() << ")";
        }
        output->displayInfo(ss.str());

        // Show progress for verbose mode
        if (options.verbose) {
            output->displayInfo("     C++ Standard: " + project->buildConfig().cppStandard());
            output->displayInfo("     Build Mode: " + Model::buildModeToString(options.mode));
            if (options.mode == Model::BuildMode::Release) {
                output->displayInfo("     Optimization: O3");
            }
        }

        // Simulate build progress
        output->startProgress("Building", 100);
        for (int i = 0; i <= 100; i += 20) {
            output->updateProgress(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        output->finishProgress();

        // Execute build
        auto result = service_->build(*project, options);

        if (result.isSuccess()) {
            // Display success message
            ss.str("");
            ss << "    Finished " << Model::buildModeToString(options.mode);
            if (options.mode == Model::BuildMode::Debug) {
                ss << " [unoptimized + debuginfo]";
            } else if (options.mode == Model::BuildMode::Release) {
                ss << " [optimized]";
            }
            ss << " target(s) in " << std::fixed << std::setprecision(2)
               << result.duration.count() / 1000.0 << "s";
            output->displayInfo(ss.str());

            // Display artifacts
            for (const auto& artifact : result.artifacts) {
                output->displaySuccess("     Created " + artifact.string());
            }
        } else {
            output->displayError("Build failed: " + result.message);
            for (const auto& error : result.errors) {
                output->displayError("  " + error);
            }
        }

    } catch (const std::exception& e) {
        output->displayError(std::string("Build failed: ") + e.what());
    }
}

CommandOptions BuildOperation::describeOptions() const
{
    return CommandOptions()
        .addFlag("release", "Build in release mode (optimized)")
        .addFlag("debug", "Build in debug mode [default]")
        .addFlag("verbose", "v", "Use verbose output")
        .addFlag("clean", "Clean before building")
        .addOption(CommandOption("target", "Build only the specified target", OptionType::String))
        .addOption(CommandOption("j", "Number of parallel jobs", OptionType::Integer));
}

void BuildOperation::displayHelp() const
{
    // This method is deprecated and will be removed
    // Help is now generated automatically from describeOptions()
}

} // namespace
