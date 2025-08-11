#pragma once

#include "ToolchainReference.h"
#include <string>
#include <optional>
#include <map>
#include <vector>

namespace scrap::configuration::model {

/**
 * @brief Project type enumeration
 */
enum class ProjectType {
    Application,
    Library
};

inline std::string toString(ProjectType type) {
    switch (type) {
        case ProjectType::Application: return "app";
        case ProjectType::Library: return "lib";
    }
    return "unknown";
}

inline ProjectType parseProjectType(const std::string& str) {
    if (str == "app" || str == "application") {
        return ProjectType::Application;
    }
    if (str == "lib" || str == "library") {
        return ProjectType::Library;
    }
    throw std::invalid_argument("Invalid project type: " + str);
}

/**
 * @brief Build system enumeration
 */
enum class BuildSystem {
    Native,  // scrap's native build system
    CMake,   // Wrapper mode for CMake
    Meson,   // Wrapper mode for Meson
    Bazel    // Wrapper mode for Bazel
};

inline std::string toString(BuildSystem system) {
    switch (system) {
        case BuildSystem::Native: return "native";
        case BuildSystem::CMake: return "cmake";
        case BuildSystem::Meson: return "meson";
        case BuildSystem::Bazel: return "bazel";
    }
    return "unknown";
}

inline BuildSystem parseBuildSystem(const std::string& str) {
    if (str == "native" || str == "scrap") {
        return BuildSystem::Native;
    }
    if (str == "cmake") {
        return BuildSystem::CMake;
    }
    if (str == "meson") {
        return BuildSystem::Meson;
    }
    if (str == "bazel") {
        return BuildSystem::Bazel;
    }
    throw std::invalid_argument("Invalid build system: " + str);
}

/**
 * @brief Configuration loaded from scrap.toml
 *
 * Represents the parsed content of a project's scrap.toml file
 * following the schema defined in CLAUDE.md.
 */
class ProjectConfiguration {
public:
    // Project metadata
    std::string name;
    std::string version = "0.1.0";
    ProjectType type = ProjectType::Application;
    std::string cppStandard = "23";

    // Build configuration
    BuildSystem buildSystem = BuildSystem::Native;
    std::optional<ToolchainReference> toolchain;

    // Build options
    std::vector<std::string> cxxFlags;
    std::vector<std::string> linkFlags;
    std::map<std::string, std::string> buildOptions;

    // Dependencies
    std::map<std::string, std::string> dependencies;
    std::map<std::string, std::string> devDependencies;

    // Test configuration
    std::string testFramework = "scrap";  // Default to built-in framework
    std::vector<std::string> testPatterns = {"*_test.cpp", "test_*.cpp"};

    // Tool configurations
    std::map<std::string, std::string> toolOptions;

    /**
     * @brief Create default configuration
     */
    static ProjectConfiguration createDefault(const std::string& projectName, ProjectType projectType) {
        ProjectConfiguration config;
        config.name = projectName;
        config.type = projectType;
        return config;
    }

    /**
     * @brief Validate configuration
     */
    void validate() const {
        if (name.empty()) {
            throw std::invalid_argument("Project name cannot be empty");
        }
        if (version.empty()) {
            throw std::invalid_argument("Project version cannot be empty");
        }
        if (cppStandard != "17" && cppStandard != "20" && cppStandard != "23") {
            throw std::invalid_argument("Unsupported C++ standard: " + cppStandard);
        }
    }

    /**
     * @brief Check if this is an application project
     */
    bool isApplication() const {
        return type == ProjectType::Application;
    }

    /**
     * @brief Check if this is a library project
     */
    bool isLibrary() const {
        return type == ProjectType::Library;
    }

    /**
     * @brief Check if using native build system
     */
    bool isNativeBuild() const {
        return buildSystem == BuildSystem::Native;
    }

    /**
     * @brief Check if using wrapper mode
     */
    bool isWrapperMode() const {
        return buildSystem != BuildSystem::Native;
    }
};

} // namespace scrap::configuration::model
