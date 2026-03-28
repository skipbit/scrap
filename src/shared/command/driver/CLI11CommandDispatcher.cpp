#include "shared/command/driver/CLI11CommandDispatcher.h"
#include "shared/command/Operation.h"
#include "shared/command/ParsedOptions.h"
#include <CLI/CLI.hpp>
#include <map>

namespace scrap {

/**
 * @brief Private implementation class for CLI11CommandDispatcher
 */
class CLI11CommandDispatcher::Impl {
public:
    std::map<std::string, std::shared_ptr<Operation>> operations_;

    CommandResult executeOperation(const std::string& command,
                                   const std::vector<std::string>& args,
                                   const ParsedOptions* options = nullptr)
    {
        // Empty command should not reach here anymore since CLI11 requires a subcommand
        // If it does, it's an error condition
        if (command.empty()) {
            return CommandResult::invalidCommand("No command specified");
        }

        auto it = operations_.find(command);
        if (it == operations_.end()) {
            return CommandResult::invalidCommand(command);
        }

        try {
            // Use parsed options if available, otherwise fall back to string args
            if (options) {
                it->second->execute(*options);
            } else {
                it->second->execute(args);
            }
            return CommandResult::success();
        } catch (const std::exception& e) {
            return CommandResult::failure(e.what());
        }
    }

    CommandResult dispatchRecursive(const CommandRequest& request)
    {
        const auto& command = request.command();

        if (request.hasSubcommand()) {
            // This is a parent command with subcommands
            // Build full command path for nested dispatch
            std::string fullCommand = command;
            for (const auto& subcommand : request.subcommands()) {
                fullCommand += "." + subcommand;
            }
            if (request.hasOptions()) {
                const ParsedOptions& options = request.options();
                return executeOperation(fullCommand, {}, &options);
            } else {
                return executeOperation(fullCommand, request.arguments());
            }
        } else {
            // Leaf command, execute directly
            if (request.hasOptions()) {
                const ParsedOptions& options = request.options();
                return executeOperation(command, {}, &options);
            } else {
                return executeOperation(command, request.arguments());
            }
        }
    }
};

// CLI11CommandDispatcher implementation
CLI11CommandDispatcher::CLI11CommandDispatcher()
    : impl_(std::make_unique<Impl>())
{
}

CLI11CommandDispatcher::~CLI11CommandDispatcher() = default;

CLI11CommandDispatcher::CLI11CommandDispatcher(CLI11CommandDispatcher&&) noexcept = default;

CLI11CommandDispatcher& CLI11CommandDispatcher::operator=(CLI11CommandDispatcher&&) noexcept = default;

CommandResult CLI11CommandDispatcher::dispatch(const CommandRequest& request)
{
    return impl_->dispatchRecursive(request);
}

void CLI11CommandDispatcher::registerOperation(const std::string& commandName, std::shared_ptr<Operation> operation)
{
    impl_->operations_[commandName] = operation;
}

}  // namespace scrap
