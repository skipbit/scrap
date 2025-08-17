#include "ConfigurationSource.h"

#include <string>

namespace scrap::Configuration::Model {

std::string toString(ConfigurationSource source) noexcept
{
    switch (source) {
        case ConfigurationSource::CommandLine:
            return "command-line";
        case ConfigurationSource::ProjectConfig:
            return "project configuration";
        case ConfigurationSource::RepositoryMarker:
            return "repository marker";
        case ConfigurationSource::Environment:
            return "environment variable";
        case ConfigurationSource::SystemDefault:
            return "system default";
    }
    return "unknown";
}

}  // namespace scrap::Configuration::Model