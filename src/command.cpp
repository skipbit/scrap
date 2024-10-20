#include "command.h"
#include "operation.h"

#include <iostream>

namespace scrap {

command::command(std::shared_ptr<operation> ptr)
    : _operation(ptr)
{
}

command::command(const command& cmd)
    : _operation(cmd._operation), _commands(cmd._commands)
{
}

command::~command() = default;

void command::add(const std::string& key, const command& cmd)
{
    _commands.insert(std::make_pair(key, cmd));
}

void command::remove(const std::string& key)
{
    _commands.erase(key);
}

void command::execute(const int argc, const char* const argv[])
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    execute(args);
}

void command::execute(const std::span<const std::string>& arguments)
{
    std::vector<command::option> options;

    _operation->setup(*this);

    if (! arguments.empty()) {
        const std::string& next_arg = arguments.front();
        if ((! next_arg.empty()) && (next_arg[0] == '-')) {
            // (wip) build options
        } else {
            const auto& i = _commands.find(next_arg);
            if (i != _commands.end()) {
                auto& [_, cmd] = *i;
                cmd.execute(arguments.subspan(1));
            } else {
                std::cerr << next_arg << ": invalid argument (operation not found)" << std::endl;
            }
            return;
        }
    }

    _operation->execute(options);
}

command& command::operator=(const command& cmd)
{
    if (this == &cmd) {
        return *this;
    }

    _operation = cmd._operation;
    _commands = cmd._commands;
    return *this;
}

command::option::option() = default;

command::option::option(const option&) = default;

command::option::~option() = default;

}
