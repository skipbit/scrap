#pragma once

#include "command/OptionSchema.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace scrap::Command {

struct ParsedOptions {
    std::unordered_map<std::string, OptionValue> named;
    std::vector<std::string> positional;
};

}  // namespace scrap::Command
