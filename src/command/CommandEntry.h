#pragma once

#include "command/CommandHandler.h"
#include "command/CommandSpec.h"
#include "command/ParsedOptions.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace scrap::Command {

enum class CommandSource {
    Builtin,
    External,
    Project
};

struct CommandEntry {
    CommandSpec spec;
    CommandSource source;
    using HandlerFactory = std::function<std::unique_ptr<CommandHandler>(const ParsedOptions&)>;
    HandlerFactory createHandler;
    std::vector<CommandEntry> subcommands;
};

}  // namespace scrap::Command
