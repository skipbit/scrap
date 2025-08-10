#include "shared/command/Command.h"
#include "shared/command/Operation.h"

#include <iostream>
#include <cstdlib>

namespace scrap {

Command::Command(std::shared_ptr<Operation> ptr)
    : operation_(ptr)
{
}

Command::Command(const Command& cmd)
    : operation_(cmd.operation_), commands_(cmd.commands_)
{
}

Command::~Command() = default;

void Command::add(const std::string& key, const Command& cmd)
{
    commands_.insert(std::make_pair(key, cmd));
}

void Command::remove(const std::string& key)
{
    commands_.erase(key);
}

void Command::execute(const int argc, const char* const argv[])
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    execute(args);
}

void Command::execute(const std::span<const std::string>& arguments)
{
    operation_->setup(*this);

    if (! arguments.empty()) {
        const std::string& next_arg = arguments.front();
        if ((! next_arg.empty()) && (next_arg[0] == '-')) {
            // (wip) build options
        } else {
            const auto& i = commands_.find(next_arg);
            if (i != commands_.end()) {
                auto& [_, cmd] = *i;
                cmd.execute(arguments.subspan(1));
            } else {
                std::cerr << next_arg << ": invalid argument (operation not found)" << std::endl;
            }
            return;
        }
    }

    std::vector<std::string> args(arguments.begin(), arguments.end());
    operation_->execute(args);
}

void Command::run(int argc, const char* const argv[])
{
    // Setup operation first to populate subcommands
    operation_->setup(*this);
    
    // Create CLI11 app directly
    CLI::App app("scrap", "Modern C++ development tool");
    app.set_help_all_flag("--help-all", "Expand all help");
    
    // Configure the app with subcommands
    configureApp(&app);
    
    try {
        app.parse(argc, argv);
        
        // Find which subcommands were parsed and dispatch
        std::vector<std::string> remainingArgs;
        dispatch(&app, remainingArgs);
    } catch (const CLI::ParseError& e) {
        std::exit(app.exit(e));
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::exit(1);
    }
}

const std::map<std::string, Command>& Command::getSubcommands() const
{
    return commands_;
}

void Command::setupOperation()
{
    operation_->setup(*this);
}

void Command::dispatch(CLI::App* app, const std::vector<std::string>& remainingArgs)
{
    // Check if any subcommands were parsed
    auto subcommands = app->get_subcommands();
    for (const auto* sub : subcommands) {
        if (sub->parsed()) {
            // Find the corresponding Command
            const auto& i = commands_.find(sub->get_name());
            if (i != commands_.end()) {
                auto& [_, cmd] = *i;
                
                // Setup subcommand first
                cmd.operation_->setup(cmd);
                
                // Get remaining arguments from CLI11
                auto remaining_args = sub->remaining();
                std::vector<std::string> subRemainingArgs(remaining_args.begin(), remaining_args.end());
                
                // Recursively dispatch to subcommand
                cmd.dispatch(const_cast<CLI::App*>(sub), subRemainingArgs);
                return;
            } else {
                std::cerr << sub->get_name() << ": invalid subcommand" << std::endl;
                return;
            }
        }
    }
    
    // No subcommands were parsed, execute this operation
    operation_->execute(remainingArgs);
}

void Command::configureApp(CLI::App* app)
{
    // Add subcommands to the CLI11 app
    for (const auto& [name, subcmd] : commands_) {
        // Add subcommand with description
        auto* sub = app->add_subcommand(name, "Subcommand: " + name);
        
        // Setup the subcommand to get its own subcommands
        const_cast<Command&>(subcmd).setupOperation();
        
        // Recursively configure nested subcommands
        const_cast<Command&>(subcmd).configureApp(sub);
    }
}

Command& Command::operator=(const Command& cmd)
{
    if (this == &cmd) {
        return *this;
    }

    operation_ = cmd.operation_;
    commands_ = cmd.commands_;
    return *this;
}


}
