#pragma once

#include "service/ConfigurationService.h"
#include <memory>

namespace scrap::Configuration {

namespace Driver {
class TomlDriver;
}

/**
 * @brief Configuration module for dependency injection and setup
 *
 * This module provides factory methods and dependency injection
 * for configuration-related services following DI principles.
 */
class ConfigurationModule {
public:
    /**
     * @brief Create default configuration service
     * @return Configured ConfigurationService instance
     */
    static std::shared_ptr<Service::ConfigurationService> createConfigurationService();

    /**
     * @brief Create configuration service with custom TOML driver
     * @param tomlDriver Custom TOML driver implementation
     * @return Configured ConfigurationService instance
     */
    static std::shared_ptr<Service::ConfigurationService>
    createConfigurationService(std::shared_ptr<Driver::TomlDriver> tomlDriver);
};

}  // namespace scrap::Configuration
