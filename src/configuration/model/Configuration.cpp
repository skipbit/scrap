#include "Configuration.h"
#include <sstream>

namespace scrap::Configuration::Model {

// Configuration class methods

const ConfigurationValue<ToolchainReference>& Configuration::toolchain() const noexcept
{
    return toolchain_;
}

const std::optional<ProjectConfiguration>& Configuration::projectConfig() const noexcept
{
    return projectConfig_;
}

void Configuration::setToolchain(ToolchainReference toolchain, ConfigurationSource source)
{
    if (!toolchain_.hasValue() || hasHigherPrecedence(source, toolchain_.source())) {
        toolchain_ = ConfigurationValue<ToolchainReference>(std::move(toolchain), source);
    }
}

void Configuration::setProjectConfig(ProjectConfiguration config)
{
    projectConfig_ = std::move(config);

    // If project config specifies a toolchain, apply it
    if (projectConfig_->toolchain) {
        setToolchain(*projectConfig_->toolchain, ConfigurationSource::ProjectConfig);
    }
}

bool Configuration::isComplete() const noexcept
{
    return toolchain_.hasValue();
}

void Configuration::applyDefaults()
{
    if (!toolchain_.hasValue()) {
        toolchain_ = ConfigurationValue<ToolchainReference>(ToolchainReference::createSystemDefault(),
                                                            ConfigurationSource::SystemDefault);
    }
}

std::string Configuration::summary() const
{
    std::ostringstream oss;

    oss << "Configuration Summary:\n";
    oss << "  Toolchain: " << toolchain_.value().toString();
    oss << " (from " << toString(toolchain_.source()) << ")\n";

    if (projectConfig_) {
        oss << "  Project: " << projectConfig_->name << " v" << projectConfig_->version << "\n";
        oss << "  Type: " << toString(projectConfig_->type) << "\n";
        oss << "  Build System: " << toString(projectConfig_->buildSystem) << "\n";
    } else {
        oss << "  Project: No scrap.toml found\n";
    }

    return oss.str();
}

}  // namespace scrap::Configuration::Model
