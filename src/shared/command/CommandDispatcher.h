#pragma once

#include "shared/command/ParsedOptions.h"
#include <string>
#include <vector>
#include <memory>

namespace scrap {

// Forward declarations
class Operation;
class CommandRequest;
class CommandResult;

/**
 * @brief Pure interface for command dispatching
 *
 * This interface defines the contract for command dispatching without
 * exposing any implementation details. It follows the Dependency Inversion
 * Principle by depending only on abstractions.
 */
class CommandDispatcher {
public:
    virtual ~CommandDispatcher() = default;

    /**
     * @brief Dispatch command based on request
     * @param request Command request containing parsed arguments and options
     * @return Command execution result
     */
    virtual CommandResult dispatch(const CommandRequest& request) = 0;

    /**
     * @brief Register an operation for a command name
     * @param commandName Name of the command
     * @param operation Operation to execute for this command
     */
    virtual void registerOperation(const std::string& commandName,
                                   std::shared_ptr<Operation> operation) = 0;
};

/**
 * @brief Command request encapsulating parsed command line input
 *
 * This class represents a parsed command request without exposing
 * the underlying CLI parsing implementation.
 */
class CommandRequest {
public:
    CommandRequest(const std::string& command,
                   const std::vector<std::string>& arguments,
                   const std::vector<std::string>& subcommands = {});

    CommandRequest(const std::string& command,
                   const ParsedOptions& options,
                   const std::vector<std::string>& subcommands = {});

    const std::string& command() const;
    const std::vector<std::string>& arguments() const;
    const std::vector<std::string>& subcommands() const;
    const ParsedOptions& options() const;
    bool hasOptions() const;

    bool hasSubcommand() const;
    CommandRequest createSubcommandRequest() const;

private:
    std::string command_;
    std::vector<std::string> arguments_;
    std::vector<std::string> subcommands_;
    std::unique_ptr<ParsedOptions> options_;
};

/**
 * @brief Result of command execution
 */
class CommandResult {
public:
    enum class Status {
        Success,
        Failure,
        InvalidCommand
    };

    CommandResult(Status status, const std::string& message = "");

    Status status() const;
    const std::string& message() const;

    static CommandResult success(const std::string& message = "");
    static CommandResult failure(const std::string& message);
    static CommandResult invalidCommand(const std::string& command);

private:
    Status status_;
    std::string message_;
};

}
