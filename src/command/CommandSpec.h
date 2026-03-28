#pragma once

#include "command/OptionSchema.h"

#include <string>
#include <vector>

namespace scrap::Command {

struct CommandSpec {
    std::string name;
    std::string description;
    std::string category;
    OptionSchema options;
    std::vector<CommandSpec> subcommands;
};

}  // namespace scrap::Command
