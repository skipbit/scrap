#pragma once

#include <dross/type/error.h>
#include <cstdint>
#include <system_error>

namespace scrap::Project::Model {

/**
 * @brief Error codes for ProjectName validation
 */
enum class ProjectNameError : std::uint8_t {
    Empty,              ///< Project name cannot be empty
    InvalidIdentifier,  ///< Project name must be a valid C++ identifier
    ReservedKeyword     ///< Project name cannot be a C++ reserved keyword
};

/**
 * @brief Error codes for Version validation
 */
enum class VersionError : std::uint8_t {
    NegativeComponent,  ///< Version components cannot be negative
    InvalidFormat       ///< Version string must match X.Y.Z format
};

/**
 * @brief Error codes for Dependency validation
 */
enum class DependencyError : std::uint8_t {
    EmptyName,     ///< Dependency name cannot be empty
    EmptyVersion   ///< Dependency version cannot be empty
};

/**
 * @brief Error codes for ProjectSpecification parsing
 */
enum class ProjectSpecificationError : std::uint8_t {
    MissingProjectName  ///< Project name is required
};

}  // namespace scrap::Project::Model

// Error code creation function declarations (must be in global scope for ADL)
// NOLINTNEXTLINE(readability-identifier-naming) - C++ standard requires this exact name for ADL
std::error_code make_error_code(scrap::Project::Model::ProjectNameError e) noexcept;

// NOLINTNEXTLINE(readability-identifier-naming) - C++ standard requires this exact name for ADL
std::error_code make_error_code(scrap::Project::Model::VersionError e) noexcept;

// NOLINTNEXTLINE(readability-identifier-naming) - C++ standard requires this exact name for ADL
std::error_code make_error_code(scrap::Project::Model::DependencyError e) noexcept;

// NOLINTNEXTLINE(readability-identifier-naming) - C++ standard requires this exact name for ADL
std::error_code make_error_code(scrap::Project::Model::ProjectSpecificationError e) noexcept;

// C++ standard requires specializing std::is_error_code_enum for custom error enums
namespace std {

template <>
struct is_error_code_enum<scrap::Project::Model::ProjectNameError> : true_type { };

template <>
struct is_error_code_enum<scrap::Project::Model::VersionError> : true_type { };

template <>
struct is_error_code_enum<scrap::Project::Model::DependencyError> : true_type { };

template <>
struct is_error_code_enum<scrap::Project::Model::ProjectSpecificationError> : true_type { };

}  // namespace std
