#pragma once

#include "ConfigurationSource.h"
#include "ProjectConfiguration.h"
#include "ToolchainReference.h"
#include <optional>
#include <string>

namespace scrap::Configuration::Model {

/**
 * @brief Configuration value with its source
 *
 * Template class that holds a configuration value along with
 * information about where it came from.
 */
template <typename T> class ConfigurationValue {
public:
    ConfigurationValue() = default;

    ConfigurationValue(T value, ConfigurationSource source);

    [[nodiscard]] const T& value() const noexcept;
    [[nodiscard]] ConfigurationSource source() const noexcept;

    [[nodiscard]] bool hasValue() const noexcept;

    // Allow implicit conversion to T for convenience
    explicit operator const T&() const noexcept;

private:
    T value_{};
    ConfigurationSource source_ = ConfigurationSource::SystemDefault;
};

// Template implementation (must be in header for templates)
template <typename T>
ConfigurationValue<T>::ConfigurationValue(T value, ConfigurationSource source)
    : value_(std::move(value)), source_(source)
{
}

template <typename T> const T& ConfigurationValue<T>::value() const noexcept
{
    return value_;
}

template <typename T> ConfigurationSource ConfigurationValue<T>::source() const noexcept
{
    return source_;
}

template <typename T> bool ConfigurationValue<T>::hasValue() const noexcept
{
    return source_ != ConfigurationSource::SystemDefault || ! value_.toString().empty();
}

template <typename T> ConfigurationValue<T>::operator const T&() const noexcept
{
    return value_;
}

/**
 * @brief Resolved configuration combining all sources
 *
 * This class represents the final configuration after applying
 * precedence rules across all configuration sources.
 */
class Configuration {
public:
    Configuration() = default;

    /**
     * @brief Get resolved toolchain reference
     */
    [[nodiscard]] const ConfigurationValue<ToolchainReference>& toolchain() const noexcept;

    /**
     * @brief Get project configuration (from scrap.toml)
     */
    [[nodiscard]] const std::optional<ProjectConfiguration>& projectConfig() const noexcept;

    /**
     * @brief Set toolchain from specific source
     */
    void setToolchain(ToolchainReference toolchain, ConfigurationSource source);

    /**
     * @brief Set project configuration
     */
    void setProjectConfig(ProjectConfiguration config);

    /**
     * @brief Check if configuration is complete
     */
    [[nodiscard]] bool isComplete() const noexcept;

    /**
     * @brief Apply system default if no toolchain specified
     */
    void applyDefaults();

    /**
     * @brief Get configuration summary for debugging
     */
    [[nodiscard]] std::string summary() const;

private:
    ConfigurationValue<ToolchainReference> toolchain_;
    std::optional<ProjectConfiguration> projectConfig_;
};

}  // namespace scrap::Configuration::Model
