#include "shared/command/Command.h"
#include "shared/command/Operation.h"

#include <iostream>

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
    std::vector<Command::Option> options;

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

    operation_->execute(options);
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

Command::Option::Option() = default;

Command::Option::Option(const Option&) = default;

Command::Option::~Option() = default;

}
