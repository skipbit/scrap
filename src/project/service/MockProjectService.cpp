#include "ProjectService.h"
#include "template/TemplateModule.h"
#include "template/service/TemplateService.h"
#include "shared/presentation/driver/ConsolePresenter.h"
#include <fstream>
#include <chrono>
#include <thread>

namespace scrap::project::service {

using namespace model;

MockProjectService::MockProjectService()
{
    presenter_ = std::make_shared<ConsolePresenter>();
    templateService_ = template_system::TemplateModule::createTemplateService(presenter_);
}

MockProjectService::MockProjectService(std::shared_ptr<template_system::service::TemplateService> templateService,
                                     std::shared_ptr<Presenter> presenter)
    : templateService_(templateService), presenter_(presenter)
{
    if (!presenter_) {
        presenter_ = std::make_shared<ConsolePresenter>();
    }
    if (!templateService_) {
        templateService_ = template_system::TemplateModule::createTemplateService(presenter_);
    }
}

Project MockProjectService::createNew(const ProjectSpecification& spec)
{
    // Validate specification
    if (spec.name.empty()) {
        throw std::runtime_error("Project name cannot be empty");
    }

    // Create project entity
    auto project = Project(
        ProjectName(spec.name),
        spec.type,
        Version(0, 1, 0)
    );

    // Set optional configurations
    if (spec.cppStandard) {
        auto config = project.buildConfig();
        config.setCppStandard(*spec.cppStandard);
        project.setBuildConfig(config);
    }

    // Add initial dependencies
    for (const auto& depName : spec.initialDependencies) {
        project.addDependency(Dependency(depName, "latest"));
    }

    // Set project path
    auto targetPath = spec.targetPath.value_or(std::filesystem::current_path() / spec.name);
    project.setPath(targetPath);

    // Use template if specified, otherwise use recommended template
    if (spec.templateName) {
        createProjectFromTemplate(spec, targetPath);
    } else {
        // Try to find default template for project type
        auto recommendedTemplate = templateService_->recommendedTemplate(
            spec.type == ProjectType::Application ? "app" : "lib");

        if (recommendedTemplate) {
            // Use recommended template
            auto modifiedSpec = spec;
            modifiedSpec.templateName = *recommendedTemplate;
            createProjectFromTemplate(modifiedSpec, targetPath);
        } else {
            // No template available
            throw std::runtime_error("No template available for project type. Please ensure templates are installed.");
        }
    }

    return project;
}

std::optional<Project> MockProjectService::loadProject(
    const std::optional<std::filesystem::path>& path)
{

    auto projectPath = path.value_or(std::filesystem::current_path());
    auto configPath = projectPath / "scrap.toml";

    // Check if scrap.toml exists
    if (!std::filesystem::exists(configPath)) {
        return std::nullopt;
    }

    // For mock implementation, create a simple project
    // In real implementation, this would parse scrap.toml
    auto project = Project(
        ProjectName("example"),
        ProjectType::Application,
        Version(0, 1, 0)
    );
    project.setPath(projectPath);

    // Add some mock dependencies
    project.addDependency(Dependency("fmt", "10.2.1"));
    project.addDependency(Dependency("spdlog", "1.13.0"));

    return project;
}

void MockProjectService::saveProject(const Project& project)
{
    if (!project.path()) {
        throw std::runtime_error("Project path not set");
    }

    generateConfigFile(project, *project.path());
}

BuildResult MockProjectService::build(const Project& project, const BuildOptions& options)
{
    // Simulate build process
    auto startTime = std::chrono::steady_clock::now();

    if (options.verbose) {
        presenter_->displayDebug("Building project: " + project.fullName());
        presenter_->displayDebug("Build mode: " + buildModeToString(options.mode));
        presenter_->displayDebug("C++ Standard: " + project.buildConfig().cppStandard());
    }

    // Simulate compilation time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Generate mock artifacts
    std::vector<std::filesystem::path> artifacts;
    if (project.path()) {
        auto buildDir = project.buildDirectory(options.mode);
        auto artifactName = project.name().toString();
        if (project.isApplication()) {
            artifacts.push_back(buildDir / artifactName);
        } else {
            artifacts.push_back(buildDir / ("lib" + artifactName + ".a"));
        }
    }

    return BuildResult::success(
        "Build completed successfully",
        duration,
        artifacts
    );
}

void MockProjectService::run(const Project& project, const RunOptions& options)
{
    if (!project.isApplication()) {
        throw std::runtime_error("Cannot run library project");
    }

    // In mock implementation, just simulate execution
    std::string command = "Running `" + project.name().toString();
    for (const auto& arg : options.arguments) {
        command += " " + arg;
    }
    command += "`";
    presenter_->displayInfo("     " + command);

    // Simulate some output
    presenter_->displayInfo("Hello, World from " + project.name().toString() + "!");
    presenter_->displayInfo("Application finished with exit code 0");
}

void MockProjectService::clean(const Project& project)
{
    if (!project.path()) {
        return;
    }

    // Simulate cleaning
    auto buildPath = *project.path() / "build";

    // In real implementation, this would actually remove files
    // For mock, we just simulate the output
    presenter_->displayInfo("     Removed " + buildPath.string());
    presenter_->displayInfo("     Cleaned build artifacts");
}

Project MockProjectService::addDependency(const Project& project,
                                           const Dependency& dependency)
{
    auto modifiedProject = project;
    modifiedProject.addDependency(dependency);
    return modifiedProject;
}

void MockProjectService::createProjectStructure(const Project& project,
                                                 const std::filesystem::path& basePath)
{
    // Create directory structure
    std::filesystem::create_directories(basePath);
    std::filesystem::create_directories(basePath / "src");
    std::filesystem::create_directories(basePath / "include" / project.name().toString());
    std::filesystem::create_directories(basePath / "tests");

    if (!project.isApplication()) {
        std::filesystem::create_directories(basePath / "examples");
    }
}

void MockProjectService::generateSourceFiles(const Project& project,
                                              const std::filesystem::path& projectPath)
{
    // Generate main source file
    auto mainFile = projectPath / "src" / "main.cpp";
    std::ofstream main(mainFile);

    if (project.isApplication()) {
        main << "#include <iostream>\n\n";
        main << "int main() {\n";
        main << "    std::cout << \"Hello, World from " << project.name().toString() << "!\" << std::endl;\n";
        main << "    return 0;\n";
        main << "}\n";
    } else {
        main << "#include \"" << project.name().toString() << "/" << project.name().toString() << ".h\"\n\n";
        main << "namespace " << project.name().toString() << " {\n\n";
        main << "void hello() {\n";
        main << "    // Implementation goes here\n";
        main << "}\n\n";
        main << "} // namespace " << project.name().toString() << "\n";
    }

    // Generate header file for library
    if (project.isLibrary()) {
        auto headerFile = projectPath / "include" / project.name().toString() /
                          (project.name().toString() + ".h");
        std::ofstream header(headerFile);

        header << "#pragma once\n\n";
        header << "namespace " << project.name().toString() << " {\n\n";
        header << "/**\n";
        header << " * @brief Example function\n";
        header << " */\n";
        header << "void hello();\n\n";
        header << "} // namespace " << project.name().toString() << "\n";
    }

    // Generate test file
    auto testFile = projectPath / "tests" / "main_test.cpp";
    std::ofstream test(testFile);

    test << "#include <cassert>\n";
    if (project.isLibrary()) {
        test << "#include \"" << project.name().toString() << "/" << project.name().toString() << ".h\"\n";
    }
    test << "\n";
    test << "int main() {\n";
    test << "    // Add your tests here\n";
    test << "    return 0;\n";
    test << "}\n";
}

void MockProjectService::generateConfigFile(const Project& project,
                                             const std::filesystem::path& projectPath)
{
    auto configFile = projectPath / "scrap.toml";
    std::ofstream config(configFile);

    config << "[package]\n";
    config << "name = \"" << project.name().toString() << "\"\n";
    config << "version = \"" << project.version().toString() << "\"\n";
    config << "type = \"" << projectTypeToString(project.type()) << "\"\n";
    config << "\n";

    config << "[build]\n";
    config << "std = \"" << project.buildConfig().cppStandard() << "\"\n";

    if (project.toolchainRequirement()) {
        config << "toolchain = \"" << *project.toolchainRequirement() << "\"\n";
    }

    config << "\n";

    if (!project.dependencies().empty()) {
        config << "[dependencies]\n";
        for (const auto& dep : project.dependencies()) {
            config << dep.name() << " = \"" << dep.version() << "\"\n";
        }
    }
}

void MockProjectService::createProjectFromTemplate(const ProjectSpecification& spec,
                                                   const std::filesystem::path& targetPath)
{
    try {
        // Load template
        std::optional<template_system::model::Template> tmpl;

        // Check if it's a local path
        if (spec.templateName->starts_with("/") || spec.templateName->starts_with("./") || spec.templateName->starts_with("../")) {
            tmpl = templateService_->loadTemplateFromPath(*spec.templateName);
        } else {
            tmpl = templateService_->loadTemplate(*spec.templateName);
        }

        if (!tmpl) {
            throw std::runtime_error("Template not found: " + *spec.templateName);
        }

        // Collect template variables
        auto variables = templateService_->collectTemplateVariables(*tmpl, spec.name);

        // Override with specification values
        variables.set("name", spec.name);
        variables.set("version", "0.1.0");

        if (spec.cppStandard) {
            variables.set("std", *spec.cppStandard);
        } else {
            variables.set("std", "23");
        }

        // Process template
        auto result = templateService_->processTemplate(*tmpl, targetPath, variables);
        if (!result) {
            throw std::runtime_error(result.error());
        }

        presenter_->displaySuccess("     Created project from template '" + *spec.templateName + "'");

    } catch (const std::exception& e) {
        presenter_->displayWarning("Failed to use template '" + *spec.templateName + "': " + e.what());
        presenter_->displayWarning("Falling back to default project generation.");

        // Fall back to hardcoded generation
        auto fallbackProject = Project(ProjectName(spec.name), spec.type, Version(0, 1, 0));
        createProjectStructure(fallbackProject, targetPath);
        generateSourceFiles(fallbackProject, targetPath);
        generateConfigFile(fallbackProject, targetPath);
    }
}

} // namespace scrap::project::service
