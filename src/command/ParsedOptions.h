#pragma once

#include "command/OptionSchema.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace scrap::Command {

struct ParsedOptions {
    std::unordered_map<std::string, OptionValue> named;

    /**
     * The positionals given on the command line, in declaration order.
     * Positionals fill from the front, so each value keeps its declared
     * index and an omitted optional positional shortens the sequence from
     * the end.
     */
    std::vector<std::string> positional;

    /**
     * The arguments after the first "--", as written, for a command that
     * takes them.
     */
    std::vector<std::string> trailing;
};

}  // namespace scrap::Command
