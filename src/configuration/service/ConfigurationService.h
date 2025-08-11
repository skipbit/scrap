#pragma once

#include "configuration/model/Configuration.h"
#include "configuration/model/ToolchainReference.h"
#include "configuration/model/ProjectConfiguration.h"
#include <optional>
#include <filesystem>
#include <memory>

namespace scrap::configuration::service {

/**
 * @brief Service interface for configuration management
 *
 * This service coordinates configuration loading from multiple sources
 * and applies precedence rules following Clean Architecture principles.
 */
class ConfigurationService {
public:
    virtual ~ConfigurationService() = default;

    /**
     * @brief Load complete configuration for current context
     * @param workingDirectory Directory to search for configuration files
     * @param cliToolchain Optional toolchain override from command line
     * @return Resolved configuration combining all sources
     */
    virtual model::Configuration loadConfiguration(
        const std::filesystem::path& workingDirectory,
        const std::optional<model::ToolchainReference>& cliToolchain = std::nullopt
    ) = 0;

    /**
     * @brief Load project configuration from scrap.toml
     * @param projectPath Path to project directory
     * @return Project configuration if scrap.toml exists
     */
    virtual std::optional<model::ProjectConfiguration> loadProjectConfiguration(
        const std::filesystem::path& projectPath
    ) = 0;

    /**
     * @brief Save project configuration to scrap.toml
     * @param projectPath Path to project directory
     * @param config Configuration to save
     */
    virtual void saveProjectConfiguration(
        const std::filesystem::path& projectPath,
        const model::ProjectConfiguration& config
    ) = 0;

    /**
     * @brief Create default scrap.toml for new project
     * @param projectPath Path to project directory
     * @param projectName Name of the project
     * @param projectType Type of project (app/lib)
     * @param toolchain Optional toolchain to specify
     */
    virtual void createDefaultConfiguration(
        const std::filesystem::path& projectPath,
        const std::string& projectName,
        model::ProjectType projectType,
        const std::optional<model::ToolchainReference>& toolchain = std::nullopt
    ) = 0;

    /**
     * @brief Set toolchain for project
     * @param projectPath Path to project directory
     * @param toolchain Toolchain to set
     */
    virtual void setProjectToolchain(
        const std::filesystem::path& projectPath,
        const model::ToolchainReference& toolchain
    ) = 0;

    /**
     * @brief Set repository-wide toolchain marker
     * @param repositoryRoot Root of the repository
     * @param toolchain Toolchain to set
     */
    virtual void setRepositoryToolchain(
        const std::filesystem::path& repositoryRoot,
        const model::ToolchainReference& toolchain
    ) = 0;

    /**
     * @brief Validate configuration
     * @param config Configuration to validate
     * @return Vector of validation error messages (empty if valid)
     */
    virtual std::vector<std::string> validateConfiguration(
        const model::Configuration& config
    ) = 0;
};

} // namespace scrap::configuration::service
