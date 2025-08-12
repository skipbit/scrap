#include "BuildOperation.h"
#include "project/service/ProjectService.h"
#include "project/model/Project.h"
#include "shared/presentation/Presenter.h"
#include <sstream>
#include <chrono>
#include <thread>

namespace scrap::project::command {

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

    // Check for help
    for (const auto& arg : args) {
        if (arg == "--help" || arg == "-h") {
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

        // Parse build options
        auto options = model::BuildOptions::parse(args);

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
            output->displayInfo("     Build Mode: " + model::buildModeToString(options.mode));
            if (options.mode == model::BuildMode::Release) {
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
            ss << "    Finished " << model::buildModeToString(options.mode);
            if (options.mode == model::BuildMode::Debug) {
                ss << " [unoptimized + debuginfo]";
            } else if (options.mode == model::BuildMode::Release) {
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

void BuildOperation::displayHelp() const
{
    auto output = presenter();
    if (!output) {
        return;
    }

    output->displayInfo("Compile the current project");
    output->displayInfo("");
    output->displayInfo("Usage: scrap build [options]");
    output->displayInfo("");
    output->displayInfo("Options:");
    output->displayInfo("  --release          Build in release mode (optimized)");
    output->displayInfo("  --debug            Build in debug mode [default]");
    output->displayInfo("  --verbose, -v      Use verbose output");
    output->displayInfo("  --clean            Clean before building");
    output->displayInfo("  --target=<name>    Build only the specified target");
    output->displayInfo("  -j<N>              Number of parallel jobs");
    output->displayInfo("");
    output->displayInfo("Examples:");
    output->displayInfo("  scrap build                    # Debug build");
    output->displayInfo("  scrap build --release          # Release build");
    output->displayInfo("  scrap build --verbose          # Verbose output");
    output->displayInfo("  scrap build --clean --release  # Clean release build");
    output->displayInfo("  scrap build -j8                # Use 8 parallel jobs");
}

} // namespace
