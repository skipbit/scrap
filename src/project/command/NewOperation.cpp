#include "NewOperation.h"
#include "project/service/ProjectService.h"
#include "project/model/Project.h"
#include "shared/presentation/Presenter.h"
#include "shared/command/CommandOptions.h"
#include "shared/command/ParsedOptions.h"
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

    if (args.empty()) {
        // No arguments provided - this should be handled by CLI11 which will
        // show help when required arguments are missing
        // Let the normal parsing flow handle this
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


CommandOptions NewOperation::describeOptions() const
{
    return CommandOptions()
        .addPositional("project-name", "Name of the new project")
        .addOption(CommandOption("type", "Project type (app, lib)", OptionType::String)
            .withDefault("app")
            .withChoices({"app", "lib"}))
        .addOption(CommandOption("template", "Use project template", OptionType::String))
        .addOption(CommandOption("path", "Target directory", OptionType::String))
        .addOption(CommandOption("std", "C++ standard version (17, 20, 23)", OptionType::String)
            .withDefault("23")
            .withChoices({"17", "20", "23"}));
}

void NewOperation::execute(const ParsedOptions& options)
{
    auto output = presenter();
    if (!output) {
        return;
    }

    // Get project name from positional argument
    auto projectName = options.string("project-name");
    if (!projectName || projectName->empty()) {
        // No project name provided - CLI11 should handle this
        output->displayError("Error: Missing required argument: <project-name>");
        output->displayInfo("Run 'scrap new --help' for usage information.");
        return;
    }

    try {
        // Build project specification from parsed options
        model::ProjectSpecification spec;
        spec.name = *projectName;

        // Parse project type
        auto typeStr = options.string("type").value_or("app");
        if (typeStr == "lib" || typeStr == "library") {
            spec.type = model::ProjectType::Library;
        } else {
            spec.type = model::ProjectType::Application;
        }

        // Set optional parameters
        spec.templateName = options.string("template");
        spec.cppStandard = options.string("std").value_or("23");

        if (auto path = options.string("path")) {
            spec.targetPath = std::filesystem::path(*path);
        }

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

} // namespace scrap::project::command
