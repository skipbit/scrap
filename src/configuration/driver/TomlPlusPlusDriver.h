#pragma once

#include "TomlDriver.h"

namespace scrap::Configuration::Driver {

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

    // Non-copyable, non-movable (base class disallows it)
    TomlPlusPlusDriver(const TomlPlusPlusDriver&) = delete;
    TomlPlusPlusDriver& operator=(const TomlPlusPlusDriver&) = delete;
    TomlPlusPlusDriver(TomlPlusPlusDriver&&) = delete;
    TomlPlusPlusDriver& operator=(TomlPlusPlusDriver&&) = delete;

    [[nodiscard]] std::expected<std::optional<Configuration::Model::ProjectConfiguration>, dross::error>
    loadProjectConfiguration(const std::filesystem::path& filePath) noexcept override;

    [[nodiscard]] std::expected<void, dross::error>
    saveProjectConfiguration(const std::filesystem::path& filePath,
                             const Configuration::Model::ProjectConfiguration& config) noexcept override;

    [[nodiscard]] std::expected<std::optional<std::map<std::string, std::string>>, dross::error>
    loadKeyValues(const std::filesystem::path& filePath) noexcept override;

    [[nodiscard]] std::expected<void, dross::error>
    saveKeyValues(const std::filesystem::path& filePath,
                  const std::map<std::string, std::string>& keyValues) noexcept override;

    [[nodiscard]] bool exists(const std::filesystem::path& filePath) override;

    std::string validateSyntax(const std::filesystem::path& filePath) override;

private:
    class Impl;
};

}  // namespace scrap::Configuration::Driver
