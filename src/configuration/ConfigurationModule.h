#pragma once

#include "service/ConfigurationService.h"
#include <memory>

namespace scrap::configuration {

namespace driver {
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
    static std::shared_ptr<service::ConfigurationService> createConfigurationService();

    /**
     * @brief Create configuration service with custom TOML driver
     * @param tomlDriver Custom TOML driver implementation
     * @return Configured ConfigurationService instance
     */
    static std::shared_ptr<service::ConfigurationService> createConfigurationService(
        std::shared_ptr<driver::TomlDriver> tomlDriver
    );
};

} // namespace scrap::configuration
