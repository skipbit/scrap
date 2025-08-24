#include "ConfigurationModule.h"
#include "driver/TomlPlusPlusDriver.h"
#include "service/DefaultConfigurationService.h"

namespace scrap::Configuration {

std::shared_ptr<Service::ConfigurationService> ConfigurationModule::createConfigurationService()
{
    // Create default TOML driver
    auto tomlDriver = std::make_shared<Driver::TomlPlusPlusDriver>();

    // Create configuration service with the driver
    return std::make_shared<Service::DefaultConfigurationService>(std::move(tomlDriver));
}

std::shared_ptr<Service::ConfigurationService>
ConfigurationModule::createConfigurationService(std::shared_ptr<Driver::TomlDriver> tomlDriver)
{
    return std::make_shared<Service::DefaultConfigurationService>(std::move(tomlDriver));
}

}  // namespace scrap::Configuration
