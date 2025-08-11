#include "shared/command/CommandDispatcher.h"

namespace scrap {

// CommandRequest implementation
CommandRequest::CommandRequest(const std::string& command,
                               const std::vector<std::string>& arguments,
                               const std::vector<std::string>& subcommands)
    : command_(command), arguments_(arguments), subcommands_(subcommands)
{
}

const std::string& CommandRequest::getCommand() const
{
    return command_;
}

const std::vector<std::string>& CommandRequest::getArguments() const
{
    return arguments_;
}

const std::vector<std::string>& CommandRequest::getSubcommands() const
{
    return subcommands_;
}

bool CommandRequest::hasSubcommand() const
{
    return !subcommands_.empty();
}

CommandRequest CommandRequest::createSubcommandRequest() const
{
    if (subcommands_.empty()) {
        return CommandRequest("", {});
    }

    std::vector<std::string> remainingSubcommands(subcommands_.begin() + 1, subcommands_.end());
    return CommandRequest(subcommands_[0], arguments_, remainingSubcommands);
}

// CommandResult implementation
CommandResult::CommandResult(Status status, const std::string& message)
    : status_(status), message_(message)
{
}

CommandResult::Status CommandResult::getStatus() const
{
    return status_;
}

const std::string& CommandResult::getMessage() const
{
    return message_;
}

CommandResult CommandResult::success(const std::string& message)
{
    return CommandResult(Status::Success, message);
}

CommandResult CommandResult::failure(const std::string& message)
{
    return CommandResult(Status::Failure, message);
}

CommandResult CommandResult::invalidCommand(const std::string& command)
{
    return CommandResult(Status::InvalidCommand, "Invalid command: " + command);
}

}
