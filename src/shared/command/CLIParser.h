#pragma once

#include "shared/command/CommandDispatcher.h"
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace scrap {

// Forward declaration
class CommandOptions;

/**
 * @brief Pure interface for CLI parsing
 *
 * This interface abstracts CLI parsing implementation details,
 * allowing different CLI libraries to be used without affecting
 * the domain layer.
 */
class CLIParser {
public:
    virtual ~CLIParser() = default;

    /**
     * @brief Parse command line arguments into a command request
     * @param args Command line arguments as a span
     * @return Parsed command request
     */
    virtual CommandRequest parse(std::span<const char* const> args) = 0;

    /**
     * @brief Configure the parser with available commands
     * @param commands List of available command names and descriptions
     */
    virtual void configureCommands(const std::vector<std::pair<std::string, std::string>>& commands) = 0;

    /**
     * @brief Add subcommand configuration
     * @param parentCommand Parent command name
     * @param subcommands List of subcommand names and descriptions
     */
    virtual void configureSubcommands(const std::string& parentCommand,
                                      const std::vector<std::pair<std::string, std::string>>& subcommands) = 0;

    /**
     * @brief Get help text for a specific command
     * @param commandPath Command path (e.g., "toolchain" or "toolchain.list")
     * @return Help text string
     */
    virtual std::string helpText(const std::string& commandPath = "") = 0;

    /**
     * @brief Configure command options from metadata
     * @param command Command name
     * @param options Command options metadata
     */
    virtual void configureCommandOptions(const std::string& command, const CommandOptions& options) = 0;
};

/**
 * @brief Factory for creating CLI parsers
 */
class CLIParserFactory {
public:
    virtual ~CLIParserFactory() = default;

    /**
     * @brief Create a CLI parser instance
     * @param appName Application name
     * @param appDescription Application description
     * @return Unique pointer to CLI parser
     */
    virtual std::unique_ptr<CLIParser> createParser(const std::string& appName, const std::string& appDescription) = 0;
};

}  // namespace scrap
