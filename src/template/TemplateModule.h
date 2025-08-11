#pragma once

#include "template/service/TemplateService.h"
#include <memory>

namespace scrap::template_system {

/**
 * @brief Template system module for dependency injection
 *
 * This module provides factory methods for creating template system components
 * and manages their lifecycle according to Clean Architecture principles.
 */
class TemplateModule {
public:
    /**
     * @brief Create default template service
     * @return Shared pointer to template service instance
     */
    static std::shared_ptr<service::TemplateService> createTemplateService();

    /**
     * @brief Create template service with custom templates directory
     * @param templatesDir Custom templates directory path
     * @return Shared pointer to template service instance
     */
    static std::shared_ptr<service::TemplateService> createTemplateService(
        const std::filesystem::path& templatesDir);

private:
    TemplateModule() = default; // Static class
};

} // namespace scrap::template_system
