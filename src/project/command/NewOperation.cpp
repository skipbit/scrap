#include "NewOperation.h"
#include "project/service/ProjectService.h"
#include "project/model/Project.h"
#include "shared/presentation/Presenter.h"
#include "template/TemplateModule.h"
#include "template/service/TemplateService.h"
#include <sstream>
#include <algorithm>

namespace scrap::project::command {

NewOperation::NewOperation(std::shared_ptr<service::ProjectService> service,
                                   std::shared_ptr<template_system::service::TemplateService> templateService)
    : service_(service), templateService_(templateService) {

    // Create default template service if not provided
    if (!templateService_) {
        templateService_ = template_system::TemplateModule::createTemplateService();
    }
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

    // Handle --list-templates flag
    if (std::find(args.begin(), args.end(), "--list-templates") != args.end()) {
        displayAvailableTemplates();
        return;
    }

    try {
        // Parse project specification
        auto spec = model::ProjectSpecification::parse(args);

        // Create the project
        auto project = service_->createNew(spec);

        // Display creation result (cargo-style)
        std::stringstream ss;
        ss << "     Created " << model::projectTypeToString(project.type())
           << " `" << project.name().toString() << "` project";
        presenter->displaySuccess(ss.str());

        // Display generated files
        presenter->displayInfo("     Generated the following files:");
        presenter->displayInfo("       " + project.name().toString() + "/");
        presenter->displayInfo("       ├── scrap.toml");
        presenter->displayInfo("       ├── src/");
        presenter->displayInfo("       │   └── main.cpp");

        if (project.isLibrary()) {
            presenter->displayInfo("       ├── include/");
            presenter->displayInfo("       │   └── " + project.name().toString() + "/");
            presenter->displayInfo("       │       └── " + project.name().toString() + ".h");
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
    presenter->displayInfo("  --type=<type>         Project type (app, lib) [default: app]");
    presenter->displayInfo("  --template=<name>     Use project template");
    presenter->displayInfo("  --path=<path>         Target directory");
    presenter->displayInfo("  --std=<version>       C++ standard (17, 20, 23) [default: 23]");
    presenter->displayInfo("  --list-templates      List available templates");
    presenter->displayInfo("");
    presenter->displayInfo("Templates:");
    presenter->displayInfo("  minimal-app           Basic C++ application (default for --type=app)");
    presenter->displayInfo("  minimal-lib           Basic C++ library (default for --type=lib)");
    presenter->displayInfo("  custom/template       Use template from custom source");
    presenter->displayInfo("  /path/to/template     Use local template directory");
    presenter->displayInfo("");
    presenter->displayInfo("Examples:");
    presenter->displayInfo("  scrap new myapp                           # Create application project");
    presenter->displayInfo("  scrap new mylib --type=lib                # Create library project");
    presenter->displayInfo("  scrap new myservice --template=minimal-app # Create from specific template");
    presenter->displayInfo("  scrap new myapp --std=20                  # Use C++20 standard");
    presenter->displayInfo("  scrap new --list-templates                # Show all available templates");
}

void NewOperation::displayAvailableTemplates() const {
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    presenter->displayInfo("Available Templates:");
    presenter->displayInfo("");

    try {
        auto templates = templateService_->listAllTemplates();

        if (templates.empty()) {
            presenter->displayInfo("  No templates found. Templates will be downloaded on first use.");
            presenter->displayInfo("");
            presenter->displayInfo("  Default templates:");
            presenter->displayInfo("    minimal-app    Basic C++ application");
            presenter->displayInfo("    minimal-lib    Basic C++ library");
            return;
        }

        // Group templates by source
        std::map<std::string, std::vector<template_system::model::Template>> templatesBySource;
        for (const auto& tmpl : templates) {
            templatesBySource[tmpl.source().name].push_back(tmpl);
        }

        for (const auto& [sourceName, sourceTemplates] : templatesBySource) {
            presenter->displayInfo("  From " + sourceName + ":");

            for (const auto& tmpl : sourceTemplates) {
                std::stringstream ss;
                ss << "    " << tmpl.name();
                if (sourceName != "official") {
                    ss << " (" << sourceName << "/" << tmpl.name() << ")";
                }
                ss << " - " << tmpl.description();
                presenter->displayInfo(ss.str());
            }
            presenter->displayInfo("");
        }

        presenter->displayInfo("Usage:");
        presenter->displayInfo("  scrap new myproject --template=<template-name>");
        presenter->displayInfo("  scrap new myproject --template=<source>/<template-name>");
        presenter->displayInfo("");
    } catch (const std::exception& e) {
        presenter->displayError("Failed to list templates: " + std::string(e.what()));
    }
}

} // namespace scrap::project::command
