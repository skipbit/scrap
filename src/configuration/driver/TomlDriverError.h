#pragma once

#include <cstdint>
#include <system_error>

namespace scrap::Configuration::Driver {

/// Error codes for TOML driver operations
enum class TomlDriverError : std::uint8_t {
    ParseError = 1,  ///< TOML parsing failed
    FileOpenError,   ///< Cannot open file for writing
    FileWriteError   ///< Error writing to file
};

}  // namespace scrap::Configuration::Driver

// ADL-discoverable make_error_code (must be in global scope)
[[nodiscard]] std::error_code make_error_code(scrap::Configuration::Driver::TomlDriverError e) noexcept;

// Enable automatic conversion to std::error_code
namespace std {
template <> struct is_error_code_enum<scrap::Configuration::Driver::TomlDriverError> : true_type { };
}  // namespace std
