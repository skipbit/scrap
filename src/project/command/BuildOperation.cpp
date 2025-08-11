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
    auto presenter = getPresenter();
    if (!presenter) {
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
            presenter->displayError("No project found in current directory");
            presenter->displayInfo("Run 'scrap new <project-name>' to create a new project");
            return;
        }

        // Parse build options
        auto options = model::BuildOptions::parse(args);

        // Display build start (cargo-style)
        if (options.clean) {
            presenter->displayInfo("   Cleaning previous build...");
            service_->clean(*project);
        }

        // Display resolving dependencies
        if (!project->dependencies().empty()) {
            presenter->displayInfo("   Resolving dependencies...");
            for (const auto& dep : project->dependencies()) {
                presenter->displaySuccess("     ✓ " + dep.name() + " " + dep.version() + " (cached)");
            }
        }

        // Start build process
        std::stringstream ss;
        ss << "   Compiling " << project->name().toString()
           << " v" << project->version().toString();
        if (project->path()) {
            ss << " (" << project->path()->string() << ")";
        }
        presenter->displayInfo(ss.str());

        // Show progress for verbose mode
        if (options.verbose) {
            presenter->displayInfo("     C++ Standard: " + project->buildConfig().cppStandard());
            presenter->displayInfo("     Build Mode: " + model::buildModeToString(options.mode));
            if (options.mode == model::BuildMode::Release) {
                presenter->displayInfo("     Optimization: O3");
            }
        }

        // Simulate build progress
        presenter->startProgress("Building", 100);
        for (int i = 0; i <= 100; i += 20) {
            presenter->updateProgress(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        presenter->finishProgress();

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
            presenter->displayInfo(ss.str());

            // Display artifacts
            for (const auto& artifact : result.artifacts) {
                presenter->displaySuccess("     Created " + artifact.string());
            }
        } else {
            presenter->displayError("Build failed: " + result.message);
            for (const auto& error : result.errors) {
                presenter->displayError("  " + error);
            }
        }

    } catch (const std::exception& e) {
        presenter->displayError(std::string("Build failed: ") + e.what());
    }
}

void BuildOperation::displayHelp() const
{
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    presenter->displayInfo("Compile the current project");
    presenter->displayInfo("");
    presenter->displayInfo("Usage: scrap build [options]");
    presenter->displayInfo("");
    presenter->displayInfo("Options:");
    presenter->displayInfo("  --release          Build in release mode (optimized)");
    presenter->displayInfo("  --debug            Build in debug mode [default]");
    presenter->displayInfo("  --verbose, -v      Use verbose output");
    presenter->displayInfo("  --clean            Clean before building");
    presenter->displayInfo("  --target=<name>    Build only the specified target");
    presenter->displayInfo("  -j<N>              Number of parallel jobs");
    presenter->displayInfo("");
    presenter->displayInfo("Examples:");
    presenter->displayInfo("  scrap build                    # Debug build");
    presenter->displayInfo("  scrap build --release          # Release build");
    presenter->displayInfo("  scrap build --verbose          # Verbose output");
    presenter->displayInfo("  scrap build --clean --release  # Clean release build");
    presenter->displayInfo("  scrap build -j8                # Use 8 parallel jobs");
}

} // namespace
