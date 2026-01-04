#include "ConfigurationError.h"
#include <string>

// Error code creation function (global scope for ADL)

std::error_code make_error_code(scrap::Configuration::Model::ProjectConfigurationError e) noexcept
{
    struct ProjectConfigurationErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "ProjectConfiguration";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::Configuration::Model::ProjectConfigurationError>(ev)) {
                case scrap::Configuration::Model::ProjectConfigurationError::InvalidProjectType:
                    return "Invalid project type";
                case scrap::Configuration::Model::ProjectConfigurationError::InvalidBuildSystem:
                    return "Invalid build system";
                case scrap::Configuration::Model::ProjectConfigurationError::EmptyProjectName:
                    return "Project name cannot be empty";
                case scrap::Configuration::Model::ProjectConfigurationError::EmptyProjectVersion:
                    return "Project version cannot be empty";
                case scrap::Configuration::Model::ProjectConfigurationError::UnsupportedCppStandard:
                    return "Unsupported C++ standard (must be 17, 20, or 23)";
                default:
                    return "Unknown ProjectConfiguration error";
            }
        }
    };

    static const ProjectConfigurationErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}
