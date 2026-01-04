#include "ConfigurationServiceError.h"
#include <string>

std::error_code make_error_code(scrap::Configuration::Service::ConfigurationServiceError e) noexcept
{
    struct ConfigurationServiceErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "ConfigurationService";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::Configuration::Service::ConfigurationServiceError>(ev)) {
                case scrap::Configuration::Service::ConfigurationServiceError::ProjectConfigNotFound:
                    return "No scrap.toml found in project directory";
                case scrap::Configuration::Service::ConfigurationServiceError::RepositoryMarkerCreateFailed:
                    return "Cannot create repository toolchain marker";
                case scrap::Configuration::Service::ConfigurationServiceError::RepositoryMarkerWriteFailed:
                    return "Error writing repository toolchain marker";
                default:
                    return "Unknown ConfigurationService error";
            }
        }
    };

    static const ConfigurationServiceErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}
