#pragma once

#include "ConfigurationService.h"
#include "configuration/driver/TomlDriver.h"
#include <memory>

namespace scrap::configuration::service {

/**
 * @brief Default implementation of ConfigurationService
 *
 * This service coordinates configuration loading from multiple sources:
 * 1. Command-line arguments (highest priority)
 * 2. Project configuration (scrap.toml)
 * 3. Repository marker (.scrap-toolchain)
 * 4. Environment variables (SCRAP_TOOLCHAIN)
 * 5. System default (lowest priority)
 */
class DefaultConfigurationService : public ConfigurationService {
public:
    /**
     * @brief Constructor with TOML driver dependency injection
     * @param tomlDriver Driver for TOML file operations
     */
    explicit DefaultConfigurationService(std::shared_ptr<driver::TomlDriver> tomlDriver);
    ~DefaultConfigurationService() override = default;

    // Non-copyable, movable
    DefaultConfigurationService(const DefaultConfigurationService&) = delete;
    DefaultConfigurationService& operator=(const DefaultConfigurationService&) = delete;
    DefaultConfigurationService(DefaultConfigurationService&&) = default;
    DefaultConfigurationService& operator=(DefaultConfigurationService&&) = default;

    Configuration::Model::Configuration loadConfiguration(
        const std::filesystem::path& workingDirectory,
        const std::optional<Configuration::Model::ToolchainReference>& cliToolchain = std::nullopt
    ) override;

    std::optional<Configuration::Model::ProjectConfiguration> loadProjectConfiguration(
        const std::filesystem::path& projectPath
    ) override;

    void saveProjectConfiguration(
        const std::filesystem::path& projectPath,
        const Configuration::Model::ProjectConfiguration& config
    ) override;

    void createDefaultConfiguration(
        const std::filesystem::path& projectPath,
        const std::string& projectName,
        Configuration::Model::ProjectType projectType,
        const std::optional<Configuration::Model::ToolchainReference>& toolchain = std::nullopt
    ) override;

    void setProjectToolchain(
        const std::filesystem::path& projectPath,
        const Configuration::Model::ToolchainReference& toolchain
    ) override;

    void setRepositoryToolchain(
        const std::filesystem::path& repositoryRoot,
        const Configuration::Model::ToolchainReference& toolchain
    ) override;

    std::vector<std::string> validateConfiguration(
        const Configuration::Model::Configuration& config
    ) override;

private:
    std::shared_ptr<driver::TomlDriver> tomlDriver_;

    /**
     * @brief Load toolchain from repository marker file
     */
    std::optional<Configuration::Model::ToolchainReference> loadRepositoryToolchain(
        const std::filesystem::path& repositoryRoot
    );

    /**
     * @brief Load toolchain from environment variable
     */
    std::optional<Configuration::Model::ToolchainReference> loadEnvironmentToolchain();

    /**
     * @brief Find repository root from given directory
     */
    std::optional<std::filesystem::path> findRepositoryRoot(
        const std::filesystem::path& startPath
    );

    /**
     * @brief Find project root (directory containing scrap.toml)
     */
    std::optional<std::filesystem::path> findProjectRoot(
        const std::filesystem::path& startPath
    );

    /**
     * @brief Get environment variable value
     */
    std::optional<std::string> environmentVariable(const std::string& name);
};

} // namespace scrap::configuration::service
