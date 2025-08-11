#pragma once

#include "ConfigurationSource.h"
#include "ToolchainReference.h"
#include "ProjectConfiguration.h"
#include <optional>
#include <map>
#include <string>

namespace scrap::configuration::model {

/**
 * @brief Configuration value with its source
 *
 * Template class that holds a configuration value along with
 * information about where it came from.
 */
template<typename T>
class ConfigurationValue {
public:
    ConfigurationValue() = default;

    ConfigurationValue(T value, ConfigurationSource source)
        : value_(std::move(value)), source_(source) {}

    const T& getValue() const { return value_; }
    ConfigurationSource getSource() const { return source_; }

    bool hasValue() const { return source_ != ConfigurationSource::SystemDefault || !value_.toString().empty(); }

    // Allow implicit conversion to T for convenience
    operator const T&() const { return value_; }

private:
    T value_{};
    ConfigurationSource source_ = ConfigurationSource::SystemDefault;
};

/**
 * @brief Resolved configuration combining all sources
 *
 * This class represents the final configuration after applying
 * precedence rules across all configuration sources.
 */
class Configuration {
public:
    /**
     * @brief Get resolved toolchain reference
     */
    const ConfigurationValue<ToolchainReference>& getToolchain() const {
        return toolchain_;
    }

    /**
     * @brief Get project configuration (from scrap.toml)
     */
    const std::optional<ProjectConfiguration>& getProjectConfig() const {
        return projectConfig_;
    }

    /**
     * @brief Set toolchain from specific source
     */
    void setToolchain(ToolchainReference toolchain, ConfigurationSource source) {
        if (!toolchain_.hasValue() || hasHigherPrecedence(source, toolchain_.getSource())) {
            toolchain_ = ConfigurationValue<ToolchainReference>(std::move(toolchain), source);
        }
    }

    /**
     * @brief Set project configuration
     */
    void setProjectConfig(ProjectConfiguration config) {
        projectConfig_ = std::move(config);

        // If project config specifies a toolchain, apply it
        if (config.toolchain) {
            setToolchain(*config.toolchain, ConfigurationSource::ProjectConfig);
        }
    }

    /**
     * @brief Check if configuration is complete
     */
    bool isComplete() const {
        return toolchain_.hasValue();
    }

    /**
     * @brief Apply system default if no toolchain specified
     */
    void applyDefaults() {
        if (!toolchain_.hasValue()) {
            toolchain_ = ConfigurationValue<ToolchainReference>(
                ToolchainReference::createSystemDefault(),
                ConfigurationSource::SystemDefault
            );
        }
    }

    /**
     * @brief Get configuration summary for debugging
     */
    std::string getSummary() const {
        std::string summary;

        summary += "Configuration Summary:\n";
        summary += "  Toolchain: " + toolchain_.getValue().toString();
        summary += " (from " + toString(toolchain_.getSource()) + ")\n";

        if (projectConfig_) {
            summary += "  Project: " + projectConfig_->name + " v" + projectConfig_->version + "\n";
            summary += "  Type: " + toString(projectConfig_->type) + "\n";
            summary += "  Build System: " + toString(projectConfig_->buildSystem) + "\n";
        } else {
            summary += "  Project: No scrap.toml found\n";
        }

        return summary;
    }

private:
    ConfigurationValue<ToolchainReference> toolchain_;
    std::optional<ProjectConfiguration> projectConfig_;
};

} // namespace scrap::configuration::model
