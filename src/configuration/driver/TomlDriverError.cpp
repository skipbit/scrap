#include "TomlDriverError.h"
#include <string>

std::error_code make_error_code(scrap::Configuration::Driver::TomlDriverError e) noexcept
{
    struct TomlDriverErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "TomlDriver";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::Configuration::Driver::TomlDriverError>(ev)) {
                case scrap::Configuration::Driver::TomlDriverError::ParseError:
                    return "TOML parse error";
                case scrap::Configuration::Driver::TomlDriverError::FileOpenError:
                    return "Cannot open file for writing";
                case scrap::Configuration::Driver::TomlDriverError::FileWriteError:
                    return "Error writing to file";
                default:
                    return "Unknown TomlDriver error";
            }
        }
    };

    static const TomlDriverErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}
