#include "DefaultConfigurationService.h"
#include <fstream>
#include <cstdlib>
#include <algorithm>

namespace scrap::configuration::service {

DefaultConfigurationService::DefaultConfigurationService(
    std::shared_ptr<driver::TomlDriver> tomlDriver)
    : tomlDriver_(std::move(tomlDriver))
{
}

Configuration::Model::Configuration DefaultConfigurationService::loadConfiguration(
    const std::filesystem::path& workingDirectory,
    const std::optional<Configuration::Model::ToolchainReference>& cliToolchain)
{

    Configuration::Model::Configuration config;

    // 1. Command-line toolchain (highest priority)
    if (cliToolchain) {
        config.setToolchain(*cliToolchain, Configuration::Model::ConfigurationSource::CommandLine);
    }

    // 2. Project configuration (scrap.toml)
    auto projectRoot = findProjectRoot(workingDirectory);
    if (projectRoot) {
        auto projectConfig = loadProjectConfiguration(*projectRoot);
        if (projectConfig) {
            config.setProjectConfig(*projectConfig);
        }
    }

    // 3. Repository marker (.scrap-toolchain)
    auto repoRoot = findRepositoryRoot(workingDirectory);
    if (repoRoot) {
        auto repoToolchain = loadRepositoryToolchain(*repoRoot);
        if (repoToolchain) {
            config.setToolchain(*repoToolchain, Configuration::Model::ConfigurationSource::RepositoryMarker);
        }
    }

    // 4. Environment variable (SCRAP_TOOLCHAIN)
    auto envToolchain = loadEnvironmentToolchain();
    if (envToolchain) {
        config.setToolchain(*envToolchain, Configuration::Model::ConfigurationSource::Environment);
    }

    // 5. Apply system default if no toolchain specified
    config.applyDefaults();

    return config;
}

std::optional<Configuration::Model::ProjectConfiguration> DefaultConfigurationService::loadProjectConfiguration(
    const std::filesystem::path& projectPath)
{

    auto configPath = projectPath / "scrap.toml";
    return tomlDriver_->loadProjectConfiguration(configPath);
}

void DefaultConfigurationService::saveProjectConfiguration(
    const std::filesystem::path& projectPath,
    const Configuration::Model::ProjectConfiguration& config)
{

    auto configPath = projectPath / "scrap.toml";

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

    if (toolchain) {
        config.toolchain = *toolchain;
    }

    saveProjectConfiguration(projectPath, config);
}

void DefaultConfigurationService::setProjectToolchain(
    const std::filesystem::path& projectPath,
    const Configuration::Model::ToolchainReference& toolchain)
{

    // Load existing configuration or create default
    auto config = loadProjectConfiguration(projectPath);
    if (!config) {
        throw std::runtime_error("No scrap.toml found in project directory");
    }

    // Update toolchain
    config->toolchain = toolchain;

    // Save updated configuration
    saveProjectConfiguration(projectPath, *config);
}

void DefaultConfigurationService::setRepositoryToolchain(
    const std::filesystem::path& repositoryRoot,
    const Configuration::Model::ToolchainReference& toolchain)
{

    auto markerPath = repositoryRoot / ".scrap-toolchain";

    // Write toolchain specification to marker file
    std::ofstream file(markerPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create repository toolchain marker: " + markerPath.string());
    }

    file << toolchain.toString() << std::endl;

    if (!file.good()) {
        throw std::runtime_error("Error writing repository toolchain marker: " + markerPath.string());
    }
}

std::vector<std::string> DefaultConfigurationService::validateConfiguration(
    const Configuration::Model::Configuration& config)
{

    std::vector<std::string> errors;

    // Validate toolchain
    if (!config.toolchain().hasValue()) {
        errors.push_back("No toolchain specified");
    }

    // Validate project configuration
    auto projectConfig = config.projectConfig();
    if (projectConfig) {
        try {
            projectConfig->validate();
        } catch (const std::exception& e) {
            errors.push_back("Project configuration error: " + std::string(e.what()));
        }
    }

    return errors;
}

std::optional<Configuration::Model::ToolchainReference> DefaultConfigurationService::loadRepositoryToolchain(
    const std::filesystem::path& repositoryRoot)
{

    auto markerPath = repositoryRoot / ".scrap-toolchain";

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
        line.erase(line.begin(), std::find_if(line.begin(), line.end(),
            [](unsigned char ch) { return !std::isspace(ch); }));
        line.erase(std::find_if(line.rbegin(), line.rend(),
            [](unsigned char ch) { return !std::isspace(ch); }).base(), line.end());

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
    auto envValue = environmentVariable("SCRAP_TOOLCHAIN");
    if (!envValue || envValue->empty()) {
        return std::nullopt;
    }

    auto result = Configuration::Model::ToolchainReference::parse(*envValue);
    if (result.has_value()) {
        return result.value();
    }
    // Invalid format, ignore
    return std::nullopt;
}

std::optional<std::filesystem::path> DefaultConfigurationService::findRepositoryRoot(
    const std::filesystem::path& startPath)
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

std::optional<std::filesystem::path> DefaultConfigurationService::findProjectRoot(
    const std::filesystem::path& startPath)
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
    if (value) {
        return std::string(value);
    }
    return std::nullopt;
}

} // namespace scrap::configuration::service
