#include "shared/command/driver/CLI11Parser.h"
#include "shared/command/driver/PresenterFormatter.h"
#include "shared/command/CommandOptions.h"
#include "shared/command/ParsedOptions.h"
#include "shared/presentation/Presenter.h"
#include "shared/constants/version.h"
#include <CLI/CLI.hpp>
#include <map>
#include <unordered_map>
#include <cstdlib>

namespace scrap {

/**
 * @brief Private implementation class for CLI11Parser
 */
class CLI11Parser::Impl {
public:
    CLI::App app_;
    std::map<std::string, CLI::App*> subcommands_;
    std::unordered_map<std::string, ParsedOptions> parsedOptions_;

    // Storage for parsed values - must persist through parsing
    struct OptionStorage {
        std::unordered_map<std::string, std::string> strings;
        std::unordered_map<std::string, int> integers;
        std::unordered_map<std::string, bool> flags;
    };
    std::unordered_map<std::string, std::unique_ptr<OptionStorage>> storageMap_;

    Impl(const std::string& appName, const std::string& appDescription)
        : app_(appDescription, appName)
    {
        app_.set_help_all_flag("--help-all", "Expand all help");
        app_.set_version_flag("--version", version());

        // Require at least one subcommand and show help when missing
        app_.require_subcommand(1);
        app_.failure_message(CLI::FailureMessage::help);
    }

    void setPresenterInternal(std::shared_ptr<Presenter> presenter)
    {
        if (presenter) {
            auto formatter = std::make_shared<PresenterFormatter>(presenter);
            app_.formatter(formatter);

            // Also set formatter for all existing subcommands
            for (auto& [name, subcommand] : subcommands_) {
                subcommand->formatter(formatter);
            }
        }
    }

    CommandRequest parseInternal(int argc, const char* const argv[])
    {
        // Check for help requests before parsing to ensure our configured options are shown
        if (argc >= 3 && std::string(argv[2]) == "--help") {
            std::string commandName = argv[1];
            auto it = subcommands_.find(commandName);
            if (it != subcommands_.end()) {
                std::cout << it->second->help() << std::endl;
                std::exit(0);
            }
        }

        try {
            app_.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            // Special handling for missing subcommand - show help without error message
            if (dynamic_cast<const CLI::RequiredError*>(&e) && argc == 1) {
                // No arguments provided, just show help
                std::cout << app_.help() << std::endl;
                std::exit(0);
            }

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
        std::string currentCommand;
        while (!current->get_subcommands().empty()) {
            bool foundParsed = false;
            for (auto* sub : current->get_subcommands()) {
                if (sub->parsed()) {
                    commandPath.push_back(sub->get_name());
                    if (currentCommand.empty()) {
                        currentCommand = sub->get_name();
                    } else {
                        currentCommand += "." + sub->get_name();
                    }
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

        // Check if we have parsed options for this command
        auto optIt = parsedOptions_.find(mainCommand);
        if (optIt != parsedOptions_.end()) {
            // Copy parsed values from storage to ParsedOptions
            copyStorageToOptions(mainCommand);

            // Return request with parsed options
            return CommandRequest(mainCommand, optIt->second, subcommandPath);
        } else {
            // Return legacy request with string arguments
            return CommandRequest(mainCommand, arguments, subcommandPath);
        }
    }

    std::string helpTextInternal(const std::string& commandPath)
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

    void configureCommandOptionsInternal(const std::string& command, const CommandOptions& options)
    {
        auto it = subcommands_.find(command);
        if (it == subcommands_.end()) {
            return;
        }

        CLI::App* app = it->second;
        ParsedOptions& parsed = parsedOptions_[command];

        // Create storage for this command if it doesn't exist
        if (storageMap_.find(command) == storageMap_.end()) {
            storageMap_[command] = std::make_unique<OptionStorage>();
        }
        auto& storage = *storageMap_[command];

        // Configure positional arguments
        for (const auto& pos : options.positionals()) {
            std::string storageName = pos.name();
            storage.strings[storageName] = "";
            auto* opt = app->add_option(storageName, storage.strings[storageName], pos.description());
            if (pos.required()) {
                opt->required();
            }
            // CLI11 automatically parses into the storage variable
            // We'll copy from storage to parsed after parsing is complete
        }

        // Configure options
        for (const auto& option : options.options()) {
            std::string optName = "--" + option.name();
            std::string storageName = option.name();

            if (option.type() == OptionType::String) {
                storage.strings[storageName] = "";
                auto* opt = app->add_option(optName, storage.strings[storageName], option.description());

                if (option.defaultValue()) {
                    opt->default_val(*option.defaultValue());
                    storage.strings[storageName] = *option.defaultValue();
                    // Set default in parsed options immediately
                    parsed.set(storageName, *option.defaultValue());
                }

                if (!option.choices().empty()) {
                    opt->check(CLI::IsMember(option.choices()));
                }

                // CLI11 automatically parses into the storage variable
            } else if (option.type() == OptionType::Integer) {
                storage.integers[storageName] = 0;
                auto* opt = app->add_option(optName, storage.integers[storageName], option.description());

                if (option.defaultValue()) {
                    opt->default_val(*option.defaultValue());
                    storage.integers[storageName] = std::stoi(*option.defaultValue());
                    // Set default in parsed options immediately
                    parsed.set(storageName, storage.integers[storageName]);
                }

                // CLI11 automatically parses into the storage variable
            }
        }

        // Configure flags
        for (const auto& flag : options.flags()) {
            std::string flagName = "--" + flag.name();
            std::string storageName = flag.name();
            storage.flags[storageName] = false;

            // Add flag option
            app->add_flag(flagName, storage.flags[storageName], flag.description());

            // CLI11 doesn't support dynamic short flag names in this version
            // Short flags would need to be configured with add_flag("-h,--help", ...)
            // For now we skip short names

            // CLI11 automatically parses into the storage variable
        }

        // Don't allow extras if options are configured
        app->allow_extras(false);
    }

    void copyStorageToOptions(const std::string& command)
    {
        auto storageIt = storageMap_.find(command);
        auto optionsIt = parsedOptions_.find(command);

        if (storageIt != storageMap_.end() && optionsIt != parsedOptions_.end()) {
            auto& storage = *storageIt->second;
            auto& options = optionsIt->second;

            // Copy string values
            for (const auto& [key, value] : storage.strings) {
                if (!value.empty()) {
                    options.set(key, value);
                }
            }

            // Copy integer values
            for (const auto& [key, value] : storage.integers) {
                options.set(key, value);
            }

            // Copy flag values
            for (const auto& [key, value] : storage.flags) {
                options.set(key, value);
            }
        }
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

        // Configure specific commands to allow additional arguments
        if (name == "new" || name == "build" || name == "run" || name == "clean") {
            sub->allow_extras();
        }

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

std::string CLI11Parser::helpText(const std::string& commandPath)
{
    return impl_->helpTextInternal(commandPath);
}

void CLI11Parser::configureCommandOptions(const std::string& command,
                                         const CommandOptions& options)
{
    impl_->configureCommandOptionsInternal(command, options);
}

void CLI11Parser::setPresenter(std::shared_ptr<Presenter> presenter)
{
    impl_->setPresenterInternal(presenter);
}

// CLI11ParserFactory implementation
std::unique_ptr<CLIParser> CLI11ParserFactory::createParser(const std::string& appName,
                                                           const std::string& appDescription)
{
    return std::make_unique<CLI11Parser>(appName, appDescription);
}

} // namespace
