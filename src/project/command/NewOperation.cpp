#include "NewOperation.h"
#include "project/service/ProjectService.h"
#include "project/model/Project.h"
#include "shared/presentation/Presenter.h"
#include <sstream>

namespace scrap::project::command {

NewOperation::NewOperation(std::shared_ptr<service::ProjectService> service)
    : service_(service) {
}

void NewOperation::execute(const std::vector<std::string>& args) {
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    if (args.empty() || args[0] == "--help") {
        displayHelp();
        return;
    }

    try {
        // Parse project specification
        auto spec = model::ProjectSpecification::parse(args);

        // Create the project
        auto project = service_->createNew(spec);

        // Display creation result (cargo-style)
        std::stringstream ss;
        ss << "     Created " << model::projectTypeToString(project.getType())
           << " `" << project.getName().toString() << "` project";
        presenter->displaySuccess(ss.str());

        // Display generated files
        presenter->displayInfo("     Generated the following files:");
        presenter->displayInfo("       " + project.getName().toString() + "/");
        presenter->displayInfo("       ├── scrap.toml");
        presenter->displayInfo("       ├── src/");
        presenter->displayInfo("       │   └── main.cpp");

        if (project.isLibrary()) {
            presenter->displayInfo("       ├── include/");
            presenter->displayInfo("       │   └── " + project.getName().toString() + "/");
            presenter->displayInfo("       │       └── " + project.getName().toString() + ".h");
        }

        presenter->displayInfo("       └── tests/");
        presenter->displayInfo("           └── main_test.cpp");

        // Display template information if used
        if (spec.templateName) {
            presenter->displayInfo("");
            ss.str("");
            ss << "     Created project from template '" << *spec.templateName << "'";
            presenter->displayInfo(ss.str());
        }

        // Display dependencies if any were added
        if (!spec.initialDependencies.empty()) {
            presenter->displayInfo("     Installing template dependencies...");
            for (const auto& dep : spec.initialDependencies) {
                presenter->displaySuccess("       ✓ " + dep + " (latest)");
            }
        }

    } catch (const std::exception& e) {
        presenter->displayError(std::string("Project creation failed: ") + e.what());
    }
}

void NewOperation::displayHelp() const {
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    presenter->displayInfo("Create a new C++ project");
    presenter->displayInfo("");
    presenter->displayInfo("Usage: scrap new <project-name> [options]");
    presenter->displayInfo("");
    presenter->displayInfo("Arguments:");
    presenter->displayInfo("  <project-name>  Name of the new project");
    presenter->displayInfo("");
    presenter->displayInfo("Options:");
    presenter->displayInfo("  --type=<type>       Project type (app, lib) [default: app]");
    presenter->displayInfo("  --template=<name>   Use project template");
    presenter->displayInfo("  --path=<path>       Target directory");
    presenter->displayInfo("  --std=<version>     C++ standard (17, 20, 23) [default: 23]");
    presenter->displayInfo("");
    presenter->displayInfo("Examples:");
    presenter->displayInfo("  scrap new myapp                    # Create application project");
    presenter->displayInfo("  scrap new mylib --type=lib         # Create library project");
    presenter->displayInfo("  scrap new myservice --template=web # Create from template");
    presenter->displayInfo("  scrap new myapp --std=20           # Use C++20 standard");
}

} // namespace scrap::project::command
