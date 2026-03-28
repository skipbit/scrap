#pragma once

#include <cstdint>
#include <dross/type/error.h>
#include <system_error>

namespace scrap::Configuration::Model {

/**
 * @brief Error codes for ProjectConfiguration validation
 */
enum class ProjectConfigurationError : std::uint8_t {
    InvalidProjectType,     ///< Invalid project type string
    InvalidBuildSystem,     ///< Invalid build system string
    EmptyProjectName,       ///< Project name cannot be empty
    EmptyProjectVersion,    ///< Project version cannot be empty
    UnsupportedCppStandard  ///< C++ standard not supported (must be 17, 20, or 23)
};

}  // namespace scrap::Configuration::Model

// Error code creation function declaration (must be in global scope for ADL)
// NOLINTNEXTLINE(readability-identifier-naming) - C++ standard requires this exact name for ADL
std::error_code make_error_code(scrap::Configuration::Model::ProjectConfigurationError e) noexcept;

// C++ standard requires specializing std::is_error_code_enum for custom error enums
namespace std {

template <> struct is_error_code_enum<scrap::Configuration::Model::ProjectConfigurationError> : true_type { };

}  // namespace std
