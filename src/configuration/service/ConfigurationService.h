#pragma once

#include "configuration/model/Configuration.h"
#include "configuration/model/ProjectConfiguration.h"
#include "configuration/model/ToolchainReference.h"
#include <filesystem>
#include <optional>
#include <expected>
#include <dross/type/error.h>

namespace scrap::Configuration::Service {

/**
 * @brief Service interface for configuration management
 *
 * This service coordinates configuration loading from multiple sources
 * and applies precedence rules following Clean Architecture principles.
 */
class ConfigurationService {
public:
    ConfigurationService();
    virtual ~ConfigurationService();

    // Non-copyable, non-movable (abstract base class)
    ConfigurationService(const ConfigurationService&) = delete;
    ConfigurationService& operator=(const ConfigurationService&) = delete;
    ConfigurationService(ConfigurationService&&) = delete;
    ConfigurationService& operator=(ConfigurationService&&) = delete;

    /**
     * @brief Load complete configuration for current context
     * @param workingDirectory Directory to search for configuration files
     * @param cliToolchain Optional toolchain override from command line
     * @return Resolved configuration combining all sources
     */
    virtual Configuration::Model::Configuration
    loadConfiguration(const std::filesystem::path& workingDirectory,
                      const std::optional<Configuration::Model::ToolchainReference>& cliToolchain = std::nullopt) = 0;

    /**
     * @brief Load project configuration from scrap.toml
     * @param projectPath Path to project directory
     * @return Project configuration if scrap.toml exists
     */
    virtual std::optional<Configuration::Model::ProjectConfiguration>
    loadProjectConfiguration(const std::filesystem::path& projectPath) = 0;

    /**
     * @brief Save project configuration to scrap.toml
     * @param projectPath Path to project directory
     * @param config Configuration to save
     */
    virtual void saveProjectConfiguration(const std::filesystem::path& projectPath,
                                          const Configuration::Model::ProjectConfiguration& config) = 0;

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
        Configuration::Model::ProjectType projectType,
        const std::optional<Configuration::Model::ToolchainReference>& toolchain = std::nullopt) = 0;

    /**
     * @brief Set toolchain for project
     * @param projectPath Path to project directory
     * @param toolchain Toolchain to set
     * @return void on success, error on failure
     */
    [[nodiscard]] virtual std::expected<void, dross::error>
    setProjectToolchain(const std::filesystem::path& projectPath,
                        const Configuration::Model::ToolchainReference& toolchain) noexcept = 0;

    /**
     * @brief Set repository-wide toolchain marker
     * @param repositoryRoot Root of the repository
     * @param toolchain Toolchain to set
     * @return void on success, error on failure
     */
    [[nodiscard]] virtual std::expected<void, dross::error>
    setRepositoryToolchain(const std::filesystem::path& repositoryRoot,
                           const Configuration::Model::ToolchainReference& toolchain) noexcept = 0;

    /**
     * @brief Validate configuration
     * @param config Configuration to validate
     * @return Vector of validation error messages (empty if valid)
     */
    virtual std::vector<std::string> validateConfiguration(const Configuration::Model::Configuration& config) = 0;
};

}  // namespace scrap::Configuration::Service
