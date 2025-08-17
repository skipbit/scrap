#pragma once

#include <cstdint>
#include <string>

namespace scrap::Configuration::Model {

/**
 * @brief Source of configuration settings
 *
 * Represents the different sources where configuration can come from,
 * ordered by precedence (highest to lowest priority).
 */
enum class ConfigurationSource : std::uint8_t {
    CommandLine,       // --toolchain=... CLI flag
    ProjectConfig,     // scrap.toml file
    RepositoryMarker,  // .scrap-toolchain file
    Environment,       // SCRAP_TOOLCHAIN env var
    SystemDefault      // System toolchain (not managed by scrap)
};

/**
 * @brief Convert source to human-readable string
 * @param source Configuration source to convert
 * @return String representation of the source
 */
std::string toString(ConfigurationSource source) noexcept;

/**
 * @brief Compare sources by precedence
 * @param left First configuration source
 * @param right Second configuration source
 * @return true if left has higher precedence than right
 */
constexpr bool hasHigherPrecedence(ConfigurationSource left, ConfigurationSource right) noexcept
{
    return static_cast<int>(left) < static_cast<int>(right);
}

}  // namespace scrap::Configuration::Model
