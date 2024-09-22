#include "command.h"
#include "operation.h"

#include <iostream>

namespace scrap {

command::command() = default;

command::command(const command&) = default;

command::~command() = default;

void command::add(const std::string& key, const scrap::operation& operation)
{
    _operations.insert(std::make_pair(key, operation));
}

void command::remove(const std::string& key)
{
    _operations.erase(key);
}

void command::execute(const std::span<const std::string>& arguments)
{
    for (const auto& i : arguments) {
        if (_operations.end() != _operations.find(i)) {
            _operations[i].execute();
        } else {
            std::cerr << i << ": invalid argument (operation not found)" << std::endl;
        }
        break;
    }
}

}
