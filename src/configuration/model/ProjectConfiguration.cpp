#include "ProjectConfiguration.h"
#include <string>

namespace scrap::Configuration::Model {

std::string toString(ProjectType type)
{
    switch (type) {
        case ProjectType::Application:
            return "app";
        case ProjectType::Library:
            return "lib";
    }
    return "unknown";
}

std::expected<ProjectType, dross::error> parseProjectType(const std::string& str) noexcept
{
    if (str == "app" || str == "application") {
        return ProjectType::Application;
    }
    if (str == "lib" || str == "library") {
        return ProjectType::Library;
    }

    auto errorCode = make_error_code(ProjectConfigurationError::InvalidProjectType);
    return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
}

std::string toString(BuildSystem system)
{
    switch (system) {
        case BuildSystem::Native:
            return "native";
        case BuildSystem::CMake:
            return "cmake";
        case BuildSystem::Meson:
            return "meson";
        case BuildSystem::Bazel:
            return "bazel";
    }
    return "unknown";
}

std::expected<BuildSystem, dross::error> parseBuildSystem(const std::string& str) noexcept
{
    if (str == "native" || str == "scrap") {
        return BuildSystem::Native;
    }
    if (str == "cmake") {
        return BuildSystem::CMake;
    }
    if (str == "meson") {
        return BuildSystem::Meson;
    }
    if (str == "bazel") {
        return BuildSystem::Bazel;
    }

    auto errorCode = make_error_code(ProjectConfigurationError::InvalidBuildSystem);
    return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
}

ProjectConfiguration::ProjectConfiguration() = default;

ProjectConfiguration ProjectConfiguration::createDefault(const std::string& projectName, ProjectType projectType)
{
    ProjectConfiguration config;
    config.name = projectName;
    config.type = projectType;
    return config;
}

std::expected<void, dross::error> ProjectConfiguration::validate() const noexcept
{
    if (name.empty()) {
        auto errorCode = make_error_code(ProjectConfigurationError::EmptyProjectName);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }
    if (version.empty()) {
        auto errorCode = make_error_code(ProjectConfigurationError::EmptyProjectVersion);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }
    if (cppStandard != "17" && cppStandard != "20" && cppStandard != "23") {
        auto errorCode = make_error_code(ProjectConfigurationError::UnsupportedCppStandard);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    return {};
}

bool ProjectConfiguration::isApplication() const noexcept
{
    return type == ProjectType::Application;
}

bool ProjectConfiguration::isLibrary() const noexcept
{
    return type == ProjectType::Library;
}

bool ProjectConfiguration::isNativeBuild() const noexcept
{
    return buildSystem == BuildSystem::Native;
}

bool ProjectConfiguration::isWrapperMode() const noexcept
{
    return buildSystem != BuildSystem::Native;
}

}  // namespace scrap::Configuration::Model
