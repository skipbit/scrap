#include "ProjectConfiguration.h"
#include <stdexcept>
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

ProjectType parseProjectType(const std::string& str)
{
    if (str == "app" || str == "application") {
        return ProjectType::Application;
    }
    if (str == "lib" || str == "library") {
        return ProjectType::Library;
    }
    throw std::invalid_argument("Invalid project type: " + str);
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

BuildSystem parseBuildSystem(const std::string& str)
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
    throw std::invalid_argument("Invalid build system: " + str);
}

ProjectConfiguration::ProjectConfiguration() = default;

ProjectConfiguration ProjectConfiguration::createDefault(const std::string& projectName, ProjectType projectType)
{
    ProjectConfiguration config;
    config.name = projectName;
    config.type = projectType;
    return config;
}

void ProjectConfiguration::validate() const
{
    if (name.empty()) {
        throw std::invalid_argument("Project name cannot be empty");
    }
    if (version.empty()) {
        throw std::invalid_argument("Project version cannot be empty");
    }
    if (cppStandard != "17" && cppStandard != "20" && cppStandard != "23") {
        throw std::invalid_argument("Unsupported C++ standard: " + cppStandard);
    }
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
