#include "application.h"

#include <iostream>
#include <span>
#include <vector>

namespace scrap {

application::application() = default;

application::~application() = default;

void application::add(const std::string& key, const scrap::command& command)
{
    _commands.insert(std::make_pair(key, command));
}

void application::execute(const int argc, const char* const argv[])
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    for (auto i = args.begin(); i != args.end(); ++i) {
        if (_commands.end() != _commands.find(*i)) {
            _commands[*i].execute(std::span{args}.subspan(std::distance(args.begin(), i) + 1));
        } else {
            std::cerr << *i << ": invalid argument (command not found)" << std::endl;
        }
        break;
    }
}

}
