#pragma once

#include "template/service/TemplateService.h"
#include <memory>
#include <vector>
#include <string>

namespace scrap {
    class Presenter;
    class CLIParser;
    class CommandDispatcher;
}

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
     * @param presenter Optional presenter for output operations
     * @return Shared pointer to template service instance
     */
    static std::shared_ptr<service::TemplateService> createTemplateService(
        std::shared_ptr<Presenter> presenter = nullptr);

    /**
     * @brief Create template service with custom templates directory
     * @param templatesDir Custom templates directory path
     * @param presenter Optional presenter for output operations
     * @return Shared pointer to template service instance
     */
    static std::shared_ptr<service::TemplateService> createTemplateService(
        const std::filesystem::path& templatesDir,
        std::shared_ptr<Presenter> presenter = nullptr);

    /**
     * @brief Register template commands with the command dispatcher
     * @param dispatcher Command dispatcher to register with
     * @param parser CLI parser for command configuration
     * @param presenter Presenter for output operations
     */
    static void registerCommands(CommandDispatcher& dispatcher,
                                std::shared_ptr<CLIParser> parser,
                                std::shared_ptr<Presenter> presenter);

    /**
     * @brief Get list of available template commands
     * @return Vector of command name and description pairs
     */
    static std::vector<std::pair<std::string, std::string>> availableCommands();

    /**
     * @brief Get list of available template subcommands
     * @return Vector of subcommand name and description pairs
     */
    static std::vector<std::pair<std::string, std::string>> availableSubcommands();

private:
    TemplateModule() = default; // Static class
};

} // namespace scrap::template_system
