#include "TemplateServiceError.h"
#include <string>

std::error_code make_error_code(scrap::template_system::service::TemplateServiceError e) noexcept
{
    struct TemplateServiceErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "TemplateService";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::template_system::service::TemplateServiceError>(ev)) {
                case scrap::template_system::service::TemplateServiceError::RegistryWriteFailed:
                    return "Cannot write template registry";
                case scrap::template_system::service::TemplateServiceError::HomeDirectoryNotFound:
                    return "Cannot determine home directory";
                default:
                    return "Unknown TemplateService error";
            }
        }
    };

    static const TemplateServiceErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}
