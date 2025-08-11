#include "ConfigurationModule.h"
#include "service/DefaultConfigurationService.h"
#include "driver/TomlPlusPlusDriver.h"

namespace scrap::configuration {

std::shared_ptr<service::ConfigurationService> ConfigurationModule::createConfigurationService() {
    // Create default TOML driver
    auto tomlDriver = std::make_shared<driver::TomlPlusPlusDriver>();

    // Create configuration service with the driver
    return std::make_shared<service::DefaultConfigurationService>(std::move(tomlDriver));
}

std::shared_ptr<service::ConfigurationService> ConfigurationModule::createConfigurationService(
    std::shared_ptr<driver::TomlDriver> tomlDriver) {

    return std::make_shared<service::DefaultConfigurationService>(std::move(tomlDriver));
}

} // namespace scrap::configuration
