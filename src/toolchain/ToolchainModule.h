#pragma once

#include "shared/command/CommandDispatcher.h"
#include "shared/presentation/Presenter.h"
#include <memory>

namespace scrap {
class CLIParser;
}

namespace scrap::toolchain {

/**
 * @brief Toolchain domain module interface
 *
 * This class represents the Toolchain domain module following DDD principles.
 * It encapsulates all toolchain-related operations and provides self-registration
 * capabilities following the Open/Closed Principle.
 */
class ToolchainModule {
public:
    /**
     * @brief Register all toolchain operations with the command dispatcher
     * @param dispatcher Command dispatcher to register operations with
     * @param parser CLI parser for help generation
     * @param presenter Presenter for output formatting
     */
    static void registerCommands(CommandDispatcher& dispatcher,
                                 std::shared_ptr<CLIParser> parser,
                                 std::shared_ptr<Presenter> presenter);

    /**
     * @brief Get available toolchain commands with descriptions
     * @return Vector of command name and description pairs
     */
    static std::vector<std::pair<std::string, std::string>> getAvailableCommands();

    /**
     * @brief Get available subcommands for toolchain
     * @return Vector of subcommand name and description pairs
     */
    static std::vector<std::pair<std::string, std::string>> getAvailableSubcommands();
};

}
