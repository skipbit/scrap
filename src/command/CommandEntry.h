#pragma once

#include "command/CommandHandler.h"
#include "command/CommandSource.h"
#include "command/CommandSpec.h"
#include "command/ParsedOptions.h"

#include <functional>
#include <memory>
#include <vector>

namespace scrap::Command {

struct CommandEntry {
    CommandSpec spec;
    CommandSource source = CommandSource::Builtin;
    using HandlerFactory = std::function<std::unique_ptr<CommandHandler>(const ParsedOptions&)>;
    HandlerFactory createHandler;
    std::vector<CommandEntry> subcommands;
};

}  // namespace scrap::Command
