#pragma once

#include "shared/command/CLIParser.h"
#include "shared/command/CommandDispatcher.h"
#include "shared/presentation/Presenter.h"
#include <memory>
#include <span>
#include <string>

namespace scrap {

class Operation;

/**
 * @brief Application-level command handler orchestrating CLI parsing and command dispatch
 *
 * This class represents the Application Layer in Clean Architecture,
 * orchestrating the interaction between CLI parsing and command execution
 * without depending on implementation details.
 */
class ApplicationCommandHandler {
public:
    ApplicationCommandHandler(std::unique_ptr<CLIParser> parser,
                              std::unique_ptr<CommandDispatcher> dispatcher,
                              std::shared_ptr<Presenter> presenter);
    ~ApplicationCommandHandler();

    // Non-copyable due to unique_ptr members
    ApplicationCommandHandler(const ApplicationCommandHandler&) = delete;
    ApplicationCommandHandler& operator=(const ApplicationCommandHandler&) = delete;

    // Movable
    ApplicationCommandHandler(ApplicationCommandHandler&&) noexcept;
    ApplicationCommandHandler& operator=(ApplicationCommandHandler&&) noexcept;

    /**
     * @brief Execute command from command line arguments
     * @param args Command line arguments as a span
     * @return Exit code (0 for success, non-zero for failure)
     */
    int execute(std::span<const char* const> args);

    /**
     * @brief Register a root-level operation
     * @param commandName Command name
     * @param operation Operation to execute
     */
    void registerRootOperation(const std::string& commandName, std::shared_ptr<Operation> operation);

    /**
     * @brief Configure command structure for CLI help generation
     */
    void configureCommands();

private:
    std::unique_ptr<CLIParser> parser_;
    std::unique_ptr<CommandDispatcher> dispatcher_;
    std::shared_ptr<Presenter> presenter_;

    void setupCommandStructure();
    void registerDomainModules();
    void configureCommandOptions();
};

/**
 * @brief Factory for creating application command handlers
 */
class ApplicationCommandHandlerFactory {
public:
    /**
     * @brief Create application command handler with default implementations
     * @return Unique pointer to application command handler
     */
    static std::unique_ptr<ApplicationCommandHandler> create();

    /**
     * @brief Create application command handler with specific implementations
     * @param parserFactory Parser factory
     * @param dispatcher Command dispatcher
     * @return Unique pointer to application command handler
     */
    static std::unique_ptr<ApplicationCommandHandler> create(std::unique_ptr<CLIParserFactory> parserFactory,
                                                             std::unique_ptr<CommandDispatcher> dispatcher);
};

}  // namespace scrap
