#include "shared/command/driver/CLI11CommandDispatcher.h"
#include "shared/command/Operation.h"
#include <CLI/CLI.hpp>
#include <map>
#include <iostream>

namespace scrap {

/**
 * @brief Private implementation class for CLI11CommandDispatcher
 */
class CLI11CommandDispatcher::Impl {
public:
    std::map<std::string, std::shared_ptr<Operation>> operations_;
    
    CommandResult executeOperation(const std::string& command, 
                                   const std::vector<std::string>& args) 
    {
        // Handle empty command (root command)
        if (command.empty()) {
            // Show general help or execute default operation
            return CommandResult::success("scrap - Modern C++ development tool\n\nUsage: scrap <subcommand> [options]\n\nAvailable subcommands:\n  toolchain    Manage toolchains\n\nUse 'scrap <subcommand> --help' for more information about a subcommand.");
        }
        
        auto it = operations_.find(command);
        if (it == operations_.end()) {
            return CommandResult::invalidCommand(command);
        }
        
        try {
            it->second->execute(args);
            return CommandResult::success();
        } catch (const std::exception& e) {
            return CommandResult::failure(e.what());
        }
    }
    
    CommandResult dispatchRecursive(const CommandRequest& request)
    {
        const auto& command = request.getCommand();
        const auto& args = request.getArguments();
        
        if (request.hasSubcommand()) {
            // This is a parent command with subcommands
            // Build full command path for nested dispatch
            std::string fullCommand = command;
            for (const auto& subcommand : request.getSubcommands()) {
                fullCommand += "." + subcommand;
            }
            return executeOperation(fullCommand, args);
        } else {
            // Leaf command, execute directly
            return executeOperation(command, args);
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

void CLI11CommandDispatcher::registerOperation(const std::string& commandName,
                                               std::shared_ptr<Operation> operation)
{
    impl_->operations_[commandName] = operation;
}

}