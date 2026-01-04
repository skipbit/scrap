#pragma once

#include "configuration/model/ProjectConfiguration.h"
#include <dross/type/error.h>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace scrap::Configuration::Driver {

/**
 * @brief Abstract interface for TOML file operations
 *
 * This driver abstracts TOML parsing and serialization operations
 * following Clean Architecture principles.
 */
class TomlDriver {
public:
    // Constructor and destructor
    TomlDriver();
    virtual ~TomlDriver();

    // Deleted copy/move operations (interface should not be copied/moved)
    TomlDriver(const TomlDriver&) = delete;
    TomlDriver& operator=(const TomlDriver&) = delete;
    TomlDriver(TomlDriver&&) = delete;
    TomlDriver& operator=(TomlDriver&&) = delete;

    /**
     * @brief Load project configuration from TOML file
     * @param filePath Path to scrap.toml file
     * @return Parsed configuration, nullopt if file doesn't exist, or error if parsing fails
     */
    [[nodiscard]] virtual std::expected<std::optional<Configuration::Model::ProjectConfiguration>, dross::error>
    loadProjectConfiguration(const std::filesystem::path& filePath) noexcept = 0;

    /**
     * @brief Save project configuration to TOML file
     * @param filePath Path to scrap.toml file
     * @param config Configuration to save
     * @return void on success, error on failure
     */
    [[nodiscard]] virtual std::expected<void, dross::error>
    saveProjectConfiguration(const std::filesystem::path& filePath,
                             const Configuration::Model::ProjectConfiguration& config) noexcept = 0;

    /**
     * @brief Load simple key-value pairs from TOML file
     * @param filePath Path to TOML file
     * @return Map of key-value pairs, nullopt if file doesn't exist, or error if parsing fails
     */
    [[nodiscard]] virtual std::expected<std::optional<std::map<std::string, std::string>>, dross::error>
    loadKeyValues(const std::filesystem::path& filePath) noexcept = 0;

    /**
     * @brief Save key-value pairs to TOML file
     * @param filePath Path to TOML file
     * @param keyValues Map of key-value pairs to save
     * @return void on success, error on failure
     */
    [[nodiscard]] virtual std::expected<void, dross::error>
    saveKeyValues(const std::filesystem::path& filePath,
                  const std::map<std::string, std::string>& keyValues) noexcept = 0;

    /**
     * @brief Check if TOML file exists and is readable
     * @param filePath Path to TOML file
     * @return True if file exists and is readable
     */
    [[nodiscard]] virtual bool exists(const std::filesystem::path& filePath) = 0;

    /**
     * @brief Validate TOML syntax without full parsing
     * @param filePath Path to TOML file
     * @return Empty string if valid, error message if invalid
     */
    virtual std::string validateSyntax(const std::filesystem::path& filePath) = 0;
};

}  // namespace scrap::Configuration::Driver
