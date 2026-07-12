#include "shared/command/CommandDispatcher.h"
#include "shared/command/ParsedOptions.h"

namespace scrap {

// CommandRequest implementation
CommandRequest::CommandRequest(const std::string& command,
                               const std::vector<std::string>& arguments,
                               const std::vector<std::string>& subcommands)
    : command_(command), arguments_(arguments), subcommands_(subcommands)
{
}

CommandRequest::CommandRequest(const std::string& command,
                               const ParsedOptions& options,
                               const std::vector<std::string>& subcommands)
    : command_(command), subcommands_(subcommands), options_(std::make_unique<ParsedOptions>(options))
{
}

const std::string& CommandRequest::command() const
{
    return command_;
}

const std::vector<std::string>& CommandRequest::arguments() const
{
    return arguments_;
}

const std::vector<std::string>& CommandRequest::subcommands() const
{
    return subcommands_;
}

const ParsedOptions& CommandRequest::options() const
{
    static ParsedOptions emptyOptions;
    return options_ ? *options_ : emptyOptions;
}

bool CommandRequest::hasOptions() const
{
    return options_ != nullptr;
}

bool CommandRequest::hasSubcommand() const
{
    return ! subcommands_.empty();
}

CommandRequest CommandRequest::createSubcommandRequest() const
{
    if (subcommands_.empty()) {
        return CommandRequest("", std::vector<std::string>{});
    }

    std::vector<std::string> remainingSubcommands(subcommands_.begin() + 1, subcommands_.end());
    if (hasOptions()) {
        return CommandRequest(subcommands_[0], options(), remainingSubcommands);
    } else {
        return CommandRequest(subcommands_[0], arguments_, remainingSubcommands);
    }
}

// CommandResult implementation
CommandResult::CommandResult(Status status, const std::string& message)
    : status_(status), message_(message)
{
}

CommandResult::Status CommandResult::status() const
{
    return status_;
}

const std::string& CommandResult::message() const
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

}  // namespace scrap
