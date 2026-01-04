#include "DefaultConfigurationService.h"
#include "ConfigurationServiceError.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <ranges>
#include <dross/type/error.h>

namespace scrap::Configuration::Service {

DefaultConfigurationService::DefaultConfigurationService(std::shared_ptr<Configuration::Driver::TomlDriver> tomlDriver)
    : tomlDriver_(std::move(tomlDriver))
{
}

DefaultConfigurationService::~DefaultConfigurationService() = default;

Configuration::Model::Configuration DefaultConfigurationService::loadConfiguration(
    const std::filesystem::path& workingDirectory,
    const std::optional<Configuration::Model::ToolchainReference>& cliToolchain)
{
    Configuration::Model::Configuration config;

    // 1. Command-line toolchain (highest priority)
    if (cliToolchain.has_value()) {
        config.setToolchain(*cliToolchain, Configuration::Model::ConfigurationSource::CommandLine);
    }

    // 2. Project configuration (scrap.toml)
    const auto projectRoot = findProjectRoot(workingDirectory);
    if (projectRoot.has_value()) {
        const auto projectConfig = loadProjectConfiguration(*projectRoot);
        if (projectConfig.has_value()) {
            config.setProjectConfig(*projectConfig);
        }
    }

    // 3. Repository marker (.scrap-toolchain)
    const auto repoRoot = findRepositoryRoot(workingDirectory);
    if (repoRoot.has_value()) {
        const auto repoToolchain = loadRepositoryToolchain(*repoRoot);
        if (repoToolchain.has_value()) {
            config.setToolchain(*repoToolchain, Configuration::Model::ConfigurationSource::RepositoryMarker);
        }
    }

    // 4. Environment variable (SCRAP_TOOLCHAIN)
    const auto envToolchain = loadEnvironmentToolchain();
    if (envToolchain.has_value()) {
        config.setToolchain(*envToolchain, Configuration::Model::ConfigurationSource::Environment);
    }

    // 5. Apply system default if no toolchain specified
    config.applyDefaults();

    return config;
}

std::optional<Configuration::Model::ProjectConfiguration>
DefaultConfigurationService::loadProjectConfiguration(const std::filesystem::path& projectPath)
{
    const auto configPath = projectPath / "scrap.toml";
    return tomlDriver_->loadProjectConfiguration(configPath);
}

void DefaultConfigurationService::saveProjectConfiguration(const std::filesystem::path& projectPath,
                                                           const Configuration::Model::ProjectConfiguration& config)
{
    const auto configPath = projectPath / "scrap.toml";

    // Ensure directory exists
    std::filesystem::create_directories(projectPath);

    tomlDriver_->saveProjectConfiguration(configPath, config);
}

void DefaultConfigurationService::createDefaultConfiguration(
    const std::filesystem::path& projectPath,
    const std::string& projectName,
    Configuration::Model::ProjectType projectType,
    const std::optional<Configuration::Model::ToolchainReference>& toolchain)
{
    auto config = Configuration::Model::ProjectConfiguration::createDefault(projectName, projectType);

    if (toolchain.has_value()) {
        config.toolchain = *toolchain;
    }

    saveProjectConfiguration(projectPath, config);
}

std::expected<void, dross::error>
DefaultConfigurationService::setProjectToolchain(const std::filesystem::path& projectPath,
                                                 const Configuration::Model::ToolchainReference& toolchain) noexcept
{
    // Load existing configuration or create default
    auto config = loadProjectConfiguration(projectPath);
    if (!config.has_value()) {
        auto errorCode = make_error_code(ConfigurationServiceError::ProjectConfigNotFound);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    // Update toolchain
    config->toolchain = toolchain;

    // Save updated configuration
    saveProjectConfiguration(projectPath, *config);

    return {};
}

std::expected<void, dross::error>
DefaultConfigurationService::setRepositoryToolchain(const std::filesystem::path& repositoryRoot,
                                                    const Configuration::Model::ToolchainReference& toolchain) noexcept
{
    const auto markerPath = repositoryRoot / ".scrap-toolchain";

    // Write toolchain specification to marker file
    std::ofstream file(markerPath);
    if (!file.is_open()) {
        auto errorCode = make_error_code(ConfigurationServiceError::RepositoryMarkerCreateFailed);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    file << toolchain.toString() << std::endl;

    if (!file.good()) {
        auto errorCode = make_error_code(ConfigurationServiceError::RepositoryMarkerWriteFailed);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    return {};
}

std::vector<std::string>
DefaultConfigurationService::validateConfiguration(const Configuration::Model::Configuration& config)
{
    std::vector<std::string> errors;

    // Validate toolchain
    if (!config.toolchain().hasValue()) {
        errors.emplace_back("No toolchain specified");
    }

    // Validate project configuration
    const auto& projectConfig = config.projectConfig();
    if (projectConfig.has_value()) {
        auto validationResult = projectConfig->validate();
        if (!validationResult) {
            errors.emplace_back("Project configuration error: " + std::string(validationResult.error().message()));
        }
    }

    return errors;
}

std::optional<Configuration::Model::ToolchainReference>
DefaultConfigurationService::loadRepositoryToolchain(const std::filesystem::path& repositoryRoot)
{
    const auto markerPath = repositoryRoot / ".scrap-toolchain";

    if (!std::filesystem::exists(markerPath)) {
        return std::nullopt;
    }

    std::ifstream file(markerPath);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::string line;
    if (std::getline(file, line)) {
        // Trim whitespace
        line.erase(line.begin(), std::ranges::find_if(line, [](unsigned char ch) { return !std::isspace(ch); }));
        line.erase(
            std::ranges::find_if(line | std::views::reverse, [](unsigned char ch) { return !std::isspace(ch); }).base(),
            line.end());

        if (!line.empty()) {
            auto result = Configuration::Model::ToolchainReference::parse(line);
            if (result.has_value()) {
                return result.value();
            }
            // Invalid format, ignore
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<Configuration::Model::ToolchainReference> DefaultConfigurationService::loadEnvironmentToolchain()
{
    const auto envValue = environmentVariable("SCRAP_TOOLCHAIN");
    if (!envValue.has_value() || envValue->empty()) {
        return std::nullopt;
    }

    const auto result = Configuration::Model::ToolchainReference::parse(*envValue);
    if (result.has_value()) {
        return result.value();
    }
    // Invalid format, ignore
    return std::nullopt;
}

std::optional<std::filesystem::path>
DefaultConfigurationService::findRepositoryRoot(const std::filesystem::path& startPath)
{
    auto currentPath = std::filesystem::canonical(startPath);

    while (currentPath != currentPath.root_path()) {
        if (std::filesystem::exists(currentPath / ".git")) {
            return currentPath;
        }
        currentPath = currentPath.parent_path();
    }

    return std::nullopt;
}

std::optional<std::filesystem::path>
DefaultConfigurationService::findProjectRoot(const std::filesystem::path& startPath)
{
    auto currentPath = std::filesystem::canonical(startPath);

    while (currentPath != currentPath.root_path()) {
        if (std::filesystem::exists(currentPath / "scrap.toml")) {
            return currentPath;
        }
        currentPath = currentPath.parent_path();
    }

    return std::nullopt;
}

std::optional<std::string> DefaultConfigurationService::environmentVariable(const std::string& name)
{
    const char* value = std::getenv(name.c_str());
    if (value != nullptr) {
        return std::string(value);
    }
    return std::nullopt;
}

}  // namespace scrap::Configuration::Service
