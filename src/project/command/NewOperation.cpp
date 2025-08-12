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
    : service_(service), templateService_(templateService)
{

    // Create default template service if not provided
    if (!templateService_) {
        templateService_ = template_system::TemplateModule::createTemplateService();
    }
}

void NewOperation::execute(const std::vector<std::string>& args)
{
    auto output = presenter();
    if (!output) {
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
        output->displaySuccess(ss.str());

        // Display generated files
        output->displayInfo("     Generated the following files:");
        output->displayInfo("       " + project.name().toString() + "/");
        output->displayInfo("       ├── scrap.toml");
        output->displayInfo("       ├── src/");
        output->displayInfo("       │   └── main.cpp");

        if (project.isLibrary()) {
            output->displayInfo("       ├── include/");
            output->displayInfo("       │   └── " + project.name().toString() + "/");
            output->displayInfo("       │       └── " + project.name().toString() + ".h");
        }

        output->displayInfo("       └── tests/");
        output->displayInfo("           └── main_test.cpp");

        // Display template information if used
        if (spec.templateName) {
            output->displayInfo("");
            ss.str("");
            ss << "     Created project from template '" << *spec.templateName << "'";
            output->displayInfo(ss.str());
        }

        // Display dependencies if any were added
        if (!spec.initialDependencies.empty()) {
            output->displayInfo("     Installing template dependencies...");
            for (const auto& dep : spec.initialDependencies) {
                output->displaySuccess("       ✓ " + dep + " (latest)");
            }
        }

    } catch (const std::exception& e) {
        output->displayError(std::string("Project creation failed: ") + e.what());
    }
}

void NewOperation::displayHelp() const
{
    auto output = presenter();
    if (!output) {
        return;
    }

    output->displayInfo("Create a new C++ project");
    output->displayInfo("");
    output->displayInfo("Usage: scrap new <project-name> [options]");
    output->displayInfo("");
    output->displayInfo("Arguments:");
    output->displayInfo("  <project-name>  Name of the new project");
    output->displayInfo("");
    output->displayInfo("Options:");
    output->displayInfo("  --type=<type>         Project type (app, lib) [default: app]");
    output->displayInfo("  --template=<name>     Use project template");
    output->displayInfo("  --path=<path>         Target directory");
    output->displayInfo("  --std=<version>       C++ standard (17, 20, 23) [default: 23]");
    output->displayInfo("  --list-templates      List available templates");
    output->displayInfo("");
    output->displayInfo("Templates:");
    output->displayInfo("  minimal-app           Basic C++ application (default for --type=app)");
    output->displayInfo("  minimal-lib           Basic C++ library (default for --type=lib)");
    output->displayInfo("  custom/template       Use template from custom source");
    output->displayInfo("  /path/to/template     Use local template directory");
    output->displayInfo("");
    output->displayInfo("Examples:");
    output->displayInfo("  scrap new myapp                           # Create application project");
    output->displayInfo("  scrap new mylib --type=lib                # Create library project");
    output->displayInfo("  scrap new myservice --template=minimal-app # Create from specific template");
    output->displayInfo("  scrap new myapp --std=20                  # Use C++20 standard");
    output->displayInfo("  scrap new --list-templates                # Show all available templates");
}

void NewOperation::displayAvailableTemplates() const
{
    auto output = presenter();
    if (!output) {
        return;
    }

    output->displayInfo("Available Templates:");
    output->displayInfo("");

    try {
        auto templates = templateService_->listAllTemplates();

        if (templates.empty()) {
            output->displayInfo("  No templates found. Templates will be downloaded on first use.");
            output->displayInfo("");
            output->displayInfo("  Default templates:");
            output->displayInfo("    minimal-app    Basic C++ application");
            output->displayInfo("    minimal-lib    Basic C++ library");
            return;
        }

        // Group templates by source
        std::map<std::string, std::vector<template_system::model::Template>> templatesBySource;
        for (const auto& tmpl : templates) {
            templatesBySource[tmpl.source().name].push_back(tmpl);
        }

        for (const auto& [sourceName, sourceTemplates] : templatesBySource) {
            output->displayInfo("  From " + sourceName + ":");

            for (const auto& tmpl : sourceTemplates) {
                std::stringstream ss;
                ss << "    " << tmpl.name();
                if (sourceName != "official") {
                    ss << " (" << sourceName << "/" << tmpl.name() << ")";
                }
                ss << " - " << tmpl.description();
                output->displayInfo(ss.str());
            }
            output->displayInfo("");
        }

        output->displayInfo("Usage:");
        output->displayInfo("  scrap new myproject --template=<template-name>");
        output->displayInfo("  scrap new myproject --template=<source>/<template-name>");
        output->displayInfo("");
    } catch (const std::exception& e) {
        output->displayError("Failed to list templates: " + std::string(e.what()));
    }
}

} // namespace scrap::project::command
