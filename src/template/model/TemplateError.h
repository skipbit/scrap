#pragma once

#include <cstdint>
#include <dross/type/error.h>
#include <system_error>

namespace scrap::template_system::model {

/**
 * @brief Error codes for Template validation
 */
enum class TemplateError : std::uint8_t {
    InvalidSourceType  ///< Invalid template source type string
};

}  // namespace scrap::template_system::model

// Error code creation function declaration (must be in global scope for ADL)
// NOLINTNEXTLINE(readability-identifier-naming) - C++ standard requires this exact name for ADL
std::error_code make_error_code(scrap::template_system::model::TemplateError e) noexcept;

// C++ standard requires specializing std::is_error_code_enum for custom error enums
namespace std {

template <> struct is_error_code_enum<scrap::template_system::model::TemplateError> : true_type { };

}  // namespace std
