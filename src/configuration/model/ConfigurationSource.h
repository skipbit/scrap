#pragma once

#include <string>

namespace scrap::configuration::model {

/**
 * @brief Source of configuration settings
 *
 * Represents the different sources where configuration can come from,
 * ordered by precedence (highest to lowest priority).
 */
enum class ConfigurationSource {
    CommandLine,      // --toolchain=... CLI flag
    ProjectConfig,    // scrap.toml file
    RepositoryMarker, // .scrap-toolchain file
    Environment,      // SCRAP_TOOLCHAIN env var
    SystemDefault     // System toolchain (not managed by scrap)
};

/**
 * @brief Convert source to human-readable string
 */
inline std::string toString(ConfigurationSource source) {
    switch (source) {
        case ConfigurationSource::CommandLine:
            return "command-line";
        case ConfigurationSource::ProjectConfig:
            return "project configuration";
        case ConfigurationSource::RepositoryMarker:
            return "repository marker";
        case ConfigurationSource::Environment:
            return "environment variable";
        case ConfigurationSource::SystemDefault:
            return "system default";
    }
    return "unknown";
}

/**
 * @brief Compare sources by precedence
 * @return true if left has higher precedence than right
 */
inline bool hasHigherPrecedence(ConfigurationSource left, ConfigurationSource right) {
    return static_cast<int>(left) < static_cast<int>(right);
}

} // namespace scrap::configuration::model
