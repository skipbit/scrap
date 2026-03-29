#pragma once

#include "command/OptionSchema.h"

#include <string>
#include <vector>

namespace scrap::Command {

struct CommandSpec {  // NOLINT(misc-no-recursion) — recursive tree structure
    std::string name;
    std::string description;
    std::string category;
    OptionSchema options;
    std::vector<CommandSpec> subcommands;
};

}  // namespace scrap::Command
