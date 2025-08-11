#include "DefaultConfigurationService.h"
#include <fstream>
#include <cstdlib>
#include <algorithm>

namespace scrap::configuration::service {

DefaultConfigurationService::DefaultConfigurationService(
    std::shared_ptr<driver::TomlDriver> tomlDriver)
    : tomlDriver_(std::move(tomlDriver)) {
}

model::Configuration DefaultConfigurationService::loadConfiguration(
    const std::filesystem::path& workingDirectory,
    const std::optional<model::ToolchainReference>& cliToolchain) {

    model::Configuration config;

    // 1. Command-line toolchain (highest priority)
    if (cliToolchain) {
        config.setToolchain(*cliToolchain, model::ConfigurationSource::CommandLine);
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
            config.setToolchain(*repoToolchain, model::ConfigurationSource::RepositoryMarker);
        }
    }

    // 4. Environment variable (SCRAP_TOOLCHAIN)
    auto envToolchain = loadEnvironmentToolchain();
    if (envToolchain) {
        config.setToolchain(*envToolchain, model::ConfigurationSource::Environment);
    }

    // 5. Apply system default if no toolchain specified
    config.applyDefaults();

    return config;
}

std::optional<model::ProjectConfiguration> DefaultConfigurationService::loadProjectConfiguration(
    const std::filesystem::path& projectPath) {

    auto configPath = projectPath / "scrap.toml";
    return tomlDriver_->loadProjectConfiguration(configPath);
}

void DefaultConfigurationService::saveProjectConfiguration(
    const std::filesystem::path& projectPath,
    const model::ProjectConfiguration& config) {

    auto configPath = projectPath / "scrap.toml";

    // Ensure directory exists
    std::filesystem::create_directories(projectPath);

    tomlDriver_->saveProjectConfiguration(configPath, config);
}

void DefaultConfigurationService::createDefaultConfiguration(
    const std::filesystem::path& projectPath,
    const std::string& projectName,
    model::ProjectType projectType,
    const std::optional<model::ToolchainReference>& toolchain) {

    auto config = model::ProjectConfiguration::createDefault(projectName, projectType);

    if (toolchain) {
        config.toolchain = *toolchain;
    }

    saveProjectConfiguration(projectPath, config);
}

void DefaultConfigurationService::setProjectToolchain(
    const std::filesystem::path& projectPath,
    const model::ToolchainReference& toolchain) {

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
    const model::ToolchainReference& toolchain) {

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
    const model::Configuration& config) {

    std::vector<std::string> errors;

    // Validate toolchain
    if (!config.getToolchain().hasValue()) {
        errors.push_back("No toolchain specified");
    }

    // Validate project configuration
    auto projectConfig = config.getProjectConfig();
    if (projectConfig) {
        try {
            projectConfig->validate();
        } catch (const std::exception& e) {
            errors.push_back("Project configuration error: " + std::string(e.what()));
        }
    }

    return errors;
}

std::optional<model::ToolchainReference> DefaultConfigurationService::loadRepositoryToolchain(
    const std::filesystem::path& repositoryRoot) {

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
            try {
                return model::ToolchainReference::parse(line);
            } catch (const std::exception&) {
                // Invalid format, ignore
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
}

std::optional<model::ToolchainReference> DefaultConfigurationService::loadEnvironmentToolchain() {
    auto envValue = getEnvironmentVariable("SCRAP_TOOLCHAIN");
    if (!envValue || envValue->empty()) {
        return std::nullopt;
    }

    try {
        return model::ToolchainReference::parse(*envValue);
    } catch (const std::exception&) {
        // Invalid format, ignore
        return std::nullopt;
    }
}

std::optional<std::filesystem::path> DefaultConfigurationService::findRepositoryRoot(
    const std::filesystem::path& startPath) {

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
    const std::filesystem::path& startPath) {

    auto currentPath = std::filesystem::canonical(startPath);

    while (currentPath != currentPath.root_path()) {
        if (std::filesystem::exists(currentPath / "scrap.toml")) {
            return currentPath;
        }
        currentPath = currentPath.parent_path();
    }

    return std::nullopt;
}

std::optional<std::string> DefaultConfigurationService::getEnvironmentVariable(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value) {
        return std::string(value);
    }
    return std::nullopt;
}

} // namespace scrap::configuration::service
