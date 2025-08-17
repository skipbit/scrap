#pragma once

#include "ToolchainReference.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace scrap::Configuration::Model {

/**
 * @brief Project type enumeration
 */
enum class ProjectType : std::uint8_t {
    Application,
    Library
};

[[nodiscard]] std::string toString(ProjectType type);
[[nodiscard]] ProjectType parseProjectType(const std::string& str);

/**
 * @brief Build system enumeration
 */
enum class BuildSystem : std::uint8_t {
    Native,  // scrap's native build system
    CMake,   // Wrapper mode for CMake
    Meson,   // Wrapper mode for Meson
    Bazel    // Wrapper mode for Bazel
};

[[nodiscard]] std::string toString(BuildSystem system);
[[nodiscard]] BuildSystem parseBuildSystem(const std::string& str);

/**
 * @brief Configuration loaded from scrap.toml
 *
 * Represents the parsed content of a project's scrap.toml file
 * following the schema defined in CLAUDE.md.
 */
class ProjectConfiguration {
public:
    ProjectConfiguration();

    // Project metadata
    std::string name;
    std::string version{"0.1.0"};
    ProjectType type{ProjectType::Application};
    std::string cppStandard{"23"};

    // Build configuration
    BuildSystem buildSystem{BuildSystem::Native};
    std::optional<ToolchainReference> toolchain;

    // Build options
    std::vector<std::string> cxxFlags;
    std::vector<std::string> linkFlags;
    std::map<std::string, std::string> buildOptions;

    // Dependencies
    std::map<std::string, std::string> dependencies;
    std::map<std::string, std::string> devDependencies;

    // Test configuration
    std::string testFramework{"scrap"};  // Default to built-in framework
    std::vector<std::string> testPatterns{"*_test.cpp", "test_*.cpp"};

    // Tool configurations
    std::map<std::string, std::string> toolOptions;

    /**
     * @brief Create default configuration
     */
    [[nodiscard]] static ProjectConfiguration createDefault(const std::string& projectName, ProjectType projectType);

    /**
     * @brief Validate configuration
     */
    void validate() const;

    /**
     * @brief Check if this is an application project
     */
    [[nodiscard]] bool isApplication() const noexcept;

    /**
     * @brief Check if this is a library project
     */
    [[nodiscard]] bool isLibrary() const noexcept;

    /**
     * @brief Check if using native build system
     */
    [[nodiscard]] bool isNativeBuild() const noexcept;

    /**
     * @brief Check if using wrapper mode
     */
    [[nodiscard]] bool isWrapperMode() const noexcept;
};

}  // namespace scrap::Configuration::Model
