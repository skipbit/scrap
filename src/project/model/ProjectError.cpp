#include "ProjectError.h"
#include <string>

// Error code creation functions (global scope for ADL)

std::error_code make_error_code(scrap::Project::Model::ProjectNameError e) noexcept
{
    struct ProjectNameErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "ProjectName";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::Project::Model::ProjectNameError>(ev)) {
                case scrap::Project::Model::ProjectNameError::Empty:
                    return "Project name cannot be empty";
                case scrap::Project::Model::ProjectNameError::InvalidIdentifier:
                    return "Project name must be a valid C++ identifier";
                case scrap::Project::Model::ProjectNameError::ReservedKeyword:
                    return "Project name cannot be a C++ reserved keyword";
                default:
                    return "Unknown ProjectName error";
            }
        }
    };

    static const ProjectNameErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}

std::error_code make_error_code(scrap::Project::Model::VersionError e) noexcept
{
    struct VersionErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "Version";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::Project::Model::VersionError>(ev)) {
                case scrap::Project::Model::VersionError::NegativeComponent:
                    return "Version components cannot be negative";
                case scrap::Project::Model::VersionError::InvalidFormat:
                    return "Invalid version format, expected X.Y.Z";
                default:
                    return "Unknown Version error";
            }
        }
    };

    static const VersionErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}

std::error_code make_error_code(scrap::Project::Model::DependencyError e) noexcept
{
    struct DependencyErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "Dependency";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::Project::Model::DependencyError>(ev)) {
                case scrap::Project::Model::DependencyError::EmptyName:
                    return "Dependency name cannot be empty";
                case scrap::Project::Model::DependencyError::EmptyVersion:
                    return "Dependency version cannot be empty";
                default:
                    return "Unknown Dependency error";
            }
        }
    };

    static const DependencyErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}

std::error_code make_error_code(scrap::Project::Model::ProjectSpecificationError e) noexcept
{
    struct ProjectSpecificationErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "ProjectSpecification";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::Project::Model::ProjectSpecificationError>(ev)) {
                case scrap::Project::Model::ProjectSpecificationError::MissingProjectName:
                    return "Project name is required";
                default:
                    return "Unknown ProjectSpecification error";
            }
        }
    };

    static const ProjectSpecificationErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}
