#pragma once

#include "configuration/model/ProjectConfiguration.h"
#include <filesystem>
#include <optional>
#include <string>
#include <map>

namespace scrap::configuration::driver {

/**
 * @brief Abstract interface for TOML file operations
 *
 * This driver abstracts TOML parsing and serialization operations
 * following Clean Architecture principles.
 */
class TomlDriver {
public:
    virtual ~TomlDriver() = default;

    /**
     * @brief Load project configuration from TOML file
     * @param filePath Path to scrap.toml file
     * @return Parsed configuration or nullopt if file doesn't exist
     * @throws std::runtime_error if file exists but parsing fails
     */
    virtual std::optional<model::ProjectConfiguration> loadProjectConfiguration(
        const std::filesystem::path& filePath
    ) = 0;

    /**
     * @brief Save project configuration to TOML file
     * @param filePath Path to scrap.toml file
     * @param config Configuration to save
     * @throws std::runtime_error if saving fails
     */
    virtual void saveProjectConfiguration(
        const std::filesystem::path& filePath,
        const model::ProjectConfiguration& config
    ) = 0;

    /**
     * @brief Load simple key-value pairs from TOML file
     * @param filePath Path to TOML file
     * @return Map of key-value pairs or nullopt if file doesn't exist
     * @throws std::runtime_error if file exists but parsing fails
     */
    virtual std::optional<std::map<std::string, std::string>> loadKeyValues(
        const std::filesystem::path& filePath
    ) = 0;

    /**
     * @brief Save key-value pairs to TOML file
     * @param filePath Path to TOML file
     * @param keyValues Map of key-value pairs to save
     * @throws std::runtime_error if saving fails
     */
    virtual void saveKeyValues(
        const std::filesystem::path& filePath,
        const std::map<std::string, std::string>& keyValues
    ) = 0;

    /**
     * @brief Check if TOML file exists and is readable
     * @param filePath Path to TOML file
     * @return True if file exists and is readable
     */
    virtual bool exists(const std::filesystem::path& filePath) = 0;

    /**
     * @brief Validate TOML syntax without full parsing
     * @param filePath Path to TOML file
     * @return Empty string if valid, error message if invalid
     */
    virtual std::string validateSyntax(const std::filesystem::path& filePath) = 0;
};

} // namespace scrap::configuration::driver
