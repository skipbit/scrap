#include "ProjectService.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <iostream>

namespace scrap::project::service {

using namespace model;

MockProjectService::MockProjectService() {
}

Project MockProjectService::createNew(const ProjectSpecification& spec) {
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
        auto config = project.getBuildConfig();
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

    // Create directory structure and files
    createProjectStructure(project, targetPath);
    generateSourceFiles(project, targetPath);
    generateConfigFile(project, targetPath);

    return project;
}

std::optional<Project> MockProjectService::loadProject(
    const std::optional<std::filesystem::path>& path) {

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

void MockProjectService::saveProject(const Project& project) {
    if (!project.getPath()) {
        throw std::runtime_error("Project path not set");
    }

    generateConfigFile(project, *project.getPath());
}

BuildResult MockProjectService::build(const Project& project, const BuildOptions& options) {
    // Simulate build process
    auto startTime = std::chrono::steady_clock::now();

    if (options.verbose) {
        std::cout << "[DEBUG] Building project: " << project.getFullName() << std::endl;
        std::cout << "[DEBUG] Build mode: " << buildModeToString(options.mode) << std::endl;
        std::cout << "[DEBUG] C++ Standard: " << project.getBuildConfig().getCppStandard() << std::endl;
    }

    // Simulate compilation time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Generate mock artifacts
    std::vector<std::filesystem::path> artifacts;
    if (project.getPath()) {
        auto buildDir = project.getBuildDirectory(options.mode);
        auto artifactName = project.getName().toString();
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

void MockProjectService::run(const Project& project, const RunOptions& options) {
    if (!project.isApplication()) {
        throw std::runtime_error("Cannot run library project");
    }

    // In mock implementation, just simulate execution
    std::cout << "     Running `" << project.getName().toString();
    for (const auto& arg : options.arguments) {
        std::cout << " " << arg;
    }
    std::cout << "`" << std::endl;

    // Simulate some output
    std::cout << "Hello, World from " << project.getName().toString() << "!" << std::endl;
    std::cout << "Application finished with exit code 0" << std::endl;
}

void MockProjectService::clean(const Project& project) {
    if (!project.getPath()) {
        return;
    }

    // Simulate cleaning
    auto buildPath = *project.getPath() / "build";

    // In real implementation, this would actually remove files
    // For mock, we just simulate the output
    std::cout << "     Removed " << buildPath.string() << std::endl;
    std::cout << "     Cleaned build artifacts" << std::endl;
}

Project MockProjectService::addDependency(const Project& project,
                                           const Dependency& dependency) {
    auto modifiedProject = project;
    modifiedProject.addDependency(dependency);
    return modifiedProject;
}

void MockProjectService::createProjectStructure(const Project& project,
                                                 const std::filesystem::path& basePath) {
    // Create directory structure
    std::filesystem::create_directories(basePath);
    std::filesystem::create_directories(basePath / "src");
    std::filesystem::create_directories(basePath / "include" / project.getName().toString());
    std::filesystem::create_directories(basePath / "tests");

    if (!project.isApplication()) {
        std::filesystem::create_directories(basePath / "examples");
    }
}

void MockProjectService::generateSourceFiles(const Project& project,
                                              const std::filesystem::path& projectPath) {
    // Generate main source file
    auto mainFile = projectPath / "src" / "main.cpp";
    std::ofstream main(mainFile);

    if (project.isApplication()) {
        main << "#include <iostream>\n\n";
        main << "int main() {\n";
        main << "    std::cout << \"Hello, World from " << project.getName().toString() << "!\" << std::endl;\n";
        main << "    return 0;\n";
        main << "}\n";
    } else {
        main << "#include \"" << project.getName().toString() << "/" << project.getName().toString() << ".h\"\n\n";
        main << "namespace " << project.getName().toString() << " {\n\n";
        main << "void hello() {\n";
        main << "    // Implementation goes here\n";
        main << "}\n\n";
        main << "} // namespace " << project.getName().toString() << "\n";
    }

    // Generate header file for library
    if (project.isLibrary()) {
        auto headerFile = projectPath / "include" / project.getName().toString() /
                          (project.getName().toString() + ".h");
        std::ofstream header(headerFile);

        header << "#pragma once\n\n";
        header << "namespace " << project.getName().toString() << " {\n\n";
        header << "/**\n";
        header << " * @brief Example function\n";
        header << " */\n";
        header << "void hello();\n\n";
        header << "} // namespace " << project.getName().toString() << "\n";
    }

    // Generate test file
    auto testFile = projectPath / "tests" / "main_test.cpp";
    std::ofstream test(testFile);

    test << "#include <cassert>\n";
    if (project.isLibrary()) {
        test << "#include \"" << project.getName().toString() << "/" << project.getName().toString() << ".h\"\n";
    }
    test << "\n";
    test << "int main() {\n";
    test << "    // Add your tests here\n";
    test << "    return 0;\n";
    test << "}\n";
}

void MockProjectService::generateConfigFile(const Project& project,
                                             const std::filesystem::path& projectPath) {
    auto configFile = projectPath / "scrap.toml";
    std::ofstream config(configFile);

    config << "[package]\n";
    config << "name = \"" << project.getName().toString() << "\"\n";
    config << "version = \"" << project.getVersion().toString() << "\"\n";
    config << "type = \"" << projectTypeToString(project.getType()) << "\"\n";
    config << "\n";

    config << "[build]\n";
    config << "std = \"" << project.getBuildConfig().getCppStandard() << "\"\n";

    if (project.getToolchainRequirement()) {
        config << "toolchain = \"" << *project.getToolchainRequirement() << "\"\n";
    }

    config << "\n";

    if (!project.getDependencies().empty()) {
        config << "[dependencies]\n";
        for (const auto& dep : project.getDependencies()) {
            config << dep.getName() << " = \"" << dep.getVersion() << "\"\n";
        }
    }
}

} // namespace scrap::project::service
