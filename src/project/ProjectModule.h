#pragma once

#include "shared/command/CLIParser.h"
#include "shared/command/CommandDispatcher.h"
#include "shared/presentation/Presenter.h"
#include <memory>
#include <string>
#include <vector>

namespace scrap::project {

/**
 * @brief Module for project management commands
 *
 * This module provides commands for creating, building, running, and managing
 * C++ projects using the scrap build system.
 */
class ProjectModule {
public:
    /**
     * @brief Register project commands with the dispatcher
     * @param dispatcher Command dispatcher to register with
     * @param parser CLI parser for argument processing
     * @param presenter Output presenter for user feedback
     */
    static void registerCommands(CommandDispatcher& dispatcher,
                                 std::shared_ptr<CLIParser> parser,
                                 std::shared_ptr<Presenter> presenter);

    /**
     * @brief Get list of available commands
     * @return Vector of command name and description pairs
     */
    static std::vector<std::pair<std::string, std::string>> availableCommands();
};

}  // namespace scrap::project
