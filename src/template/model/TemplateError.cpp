#include "TemplateError.h"
#include <string>

// Error code creation function (global scope for ADL)

std::error_code make_error_code(scrap::template_system::model::TemplateError e) noexcept
{
    struct TemplateErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "Template";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::template_system::model::TemplateError>(ev)) {
                case scrap::template_system::model::TemplateError::InvalidSourceType:
                    return "Invalid template source type";
                default:
                    return "Unknown Template error";
            }
        }
    };

    static const TemplateErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}
