#include "shared/command/driver/CLI11Parser.h"
#include "shared/constants/version.h"
#include <CLI/CLI.hpp>
#include <map>
#include <iostream>
#include <cstdlib>

namespace scrap {

/**
 * @brief Private implementation class for CLI11Parser
 */
class CLI11Parser::Impl {
public:
    CLI::App app_;
    std::map<std::string, CLI::App*> subcommands_;
    
    Impl(const std::string& appName, const std::string& appDescription)
        : app_(appDescription, appName)
    {
        app_.set_help_all_flag("--help-all", "Expand all help");
        app_.set_version_flag("--version", version());
    }
    
    CommandRequest parseInternal(int argc, const char* const argv[])
    {
        try {
            app_.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            // Handle help requests and other CLI11 errors
            // For help requests, CLI11 sets the exit code to 0
            // For errors, it sets non-zero exit codes
            int exitCode = app_.exit(e);
            
            // If it's a help request (exit code 0), we exit successfully
            if (exitCode == 0) {
                std::exit(0);
            } else {
                // For other errors, exit with error code
                std::exit(exitCode);
            }
        }
        
        // Find which command was parsed
        std::vector<std::string> commandPath;
        std::vector<std::string> arguments;
        
        // Check if no subcommands are available yet (empty app)
        if (subcommands_.empty()) {
            // No subcommands configured, this is a root command
            auto remaining = app_.remaining();
            arguments.assign(remaining.begin(), remaining.end());
            return CommandRequest("", arguments);
        }
        
        // Find parsed subcommand path
        CLI::App* current = &app_;
        while (!current->get_subcommands().empty()) {
            bool foundParsed = false;
            for (auto* sub : current->get_subcommands()) {
                if (sub->parsed()) {
                    commandPath.push_back(sub->get_name());
                    current = sub;
                    foundParsed = true;
                    break;
                }
            }
            if (!foundParsed) break;
        }
        
        // Get remaining arguments from the last parsed command
        auto remaining = current->remaining();
        arguments.assign(remaining.begin(), remaining.end());
        
        if (commandPath.empty()) {
            return CommandRequest("", arguments);
        }
        
        // Create hierarchical command request
        std::string mainCommand = commandPath[0];
        std::vector<std::string> subcommandPath(commandPath.begin() + 1, commandPath.end());
        
        return CommandRequest(mainCommand, arguments, subcommandPath);
    }
    
    std::string getHelpTextInternal(const std::string& commandPath)
    {
        if (commandPath.empty()) {
            return app_.help();
        }
        
        auto it = subcommands_.find(commandPath);
        if (it != subcommands_.end()) {
            return it->second->help();
        }
        
        // If not found in flat map, try hierarchical path
        size_t dotPos = commandPath.find('.');
        if (dotPos != std::string::npos) {
            std::string parentCmd = commandPath.substr(0, dotPos);
            auto parentIt = subcommands_.find(parentCmd);
            if (parentIt != subcommands_.end()) {
                return parentIt->second->help();
            }
        }
        
        return "Command not found: " + commandPath;
    }
};

// CLI11Parser implementation
CLI11Parser::CLI11Parser(const std::string& appName, const std::string& appDescription)
    : impl_(std::make_unique<Impl>(appName, appDescription))
{
}

CLI11Parser::~CLI11Parser() = default;

CLI11Parser::CLI11Parser(CLI11Parser&&) noexcept = default;

CLI11Parser& CLI11Parser::operator=(CLI11Parser&&) noexcept = default;

CommandRequest CLI11Parser::parse(int argc, const char* const argv[])
{
    return impl_->parseInternal(argc, argv);
}

void CLI11Parser::configureCommands(const std::vector<std::pair<std::string, std::string>>& commands)
{
    for (const auto& [name, description] : commands) {
        auto* sub = impl_->app_.add_subcommand(name, description);
        impl_->subcommands_[name] = sub;
    }
}

void CLI11Parser::configureSubcommands(const std::string& parentCommand,
                                       const std::vector<std::pair<std::string, std::string>>& subcommands)
{
    auto it = impl_->subcommands_.find(parentCommand);
    if (it == impl_->subcommands_.end()) {
        throw std::runtime_error("Parent command not found: " + parentCommand);
    }
    
    CLI::App* parent = it->second;
    for (const auto& [name, description] : subcommands) {
        auto* sub = parent->add_subcommand(name, description);
        std::string fullName = parentCommand + "." + name;
        impl_->subcommands_[fullName] = sub;
    }
}

std::string CLI11Parser::getHelpText(const std::string& commandPath)
{
    return impl_->getHelpTextInternal(commandPath);
}

// CLI11ParserFactory implementation
std::unique_ptr<CLIParser> CLI11ParserFactory::createParser(const std::string& appName,
                                                           const std::string& appDescription)
{
    return std::make_unique<CLI11Parser>(appName, appDescription);
}

}