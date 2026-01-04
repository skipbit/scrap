#pragma once

#include <cstdint>
#include <system_error>

namespace scrap::template_system::service {

/// Error codes for template service operations
enum class TemplateServiceError : std::uint8_t {
    RegistryWriteFailed = 1,  ///< Failed to write template registry file
    HomeDirectoryNotFound     ///< Cannot determine home directory
};

}  // namespace scrap::template_system::service

// ADL-discoverable make_error_code (must be in global scope)
[[nodiscard]] std::error_code make_error_code(scrap::template_system::service::TemplateServiceError e) noexcept;

// Enable automatic conversion to std::error_code
namespace std {
template <>
struct is_error_code_enum<scrap::template_system::service::TemplateServiceError> : true_type { };
}  // namespace std
