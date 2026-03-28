#pragma once

#include <cstdint>
#include <system_error>

namespace scrap::Configuration::Service {

/// Error codes for configuration service operations
enum class ConfigurationServiceError : std::uint8_t {
    ProjectConfigNotFound = 1,     ///< No scrap.toml found in project directory
    RepositoryMarkerCreateFailed,  ///< Cannot create repository toolchain marker file
    RepositoryMarkerWriteFailed    ///< Error writing repository toolchain marker file
};

}  // namespace scrap::Configuration::Service

// ADL-discoverable make_error_code (must be in global scope)
[[nodiscard]] std::error_code make_error_code(scrap::Configuration::Service::ConfigurationServiceError e) noexcept;

// Enable automatic conversion to std::error_code
namespace std {
template <> struct is_error_code_enum<scrap::Configuration::Service::ConfigurationServiceError> : true_type { };
}  // namespace std
