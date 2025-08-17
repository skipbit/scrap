#pragma once

#include "TomlDriver.h"
#include <memory>

namespace scrap::configuration::driver {

/**
 * @brief TOML driver implementation using toml++ library
 *
 * Concrete implementation of TomlDriver using the toml++ library
 * for parsing and serializing TOML files.
 */
class TomlPlusPlusDriver : public TomlDriver {
public:
    TomlPlusPlusDriver();
    ~TomlPlusPlusDriver() override;

    // Non-copyable, movable
    TomlPlusPlusDriver(const TomlPlusPlusDriver&) = delete;
    TomlPlusPlusDriver& operator=(const TomlPlusPlusDriver&) = delete;
    TomlPlusPlusDriver(TomlPlusPlusDriver&&) = default;
    TomlPlusPlusDriver& operator=(TomlPlusPlusDriver&&) = default;

    std::optional<Configuration::Model::ProjectConfiguration> loadProjectConfiguration(
        const std::filesystem::path& filePath
    ) override;

    void saveProjectConfiguration(
        const std::filesystem::path& filePath,
        const Configuration::Model::ProjectConfiguration& config
    ) override;

    std::optional<std::map<std::string, std::string>> loadKeyValues(
        const std::filesystem::path& filePath
    ) override;

    void saveKeyValues(
        const std::filesystem::path& filePath,
        const std::map<std::string, std::string>& keyValues
    ) override;

    bool exists(const std::filesystem::path& filePath) override;

    std::string validateSyntax(const std::filesystem::path& filePath) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace scrap::configuration::driver
