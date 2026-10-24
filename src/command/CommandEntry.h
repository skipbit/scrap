#pragma once

#include "command/CommandHandler.h"
#include "command/CommandSource.h"
#include "command/CommandSpec.h"
#include "command/ParsedOptions.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace scrap::Command {

struct CommandEntry {
    CommandSpec spec;
    CommandSource source = CommandSource::Builtin;
    std::filesystem::path location;  ///< Where an external command was found; empty for the others.
    using HandlerFactory = std::function<std::unique_ptr<CommandHandler>(const ParsedOptions&)>;
    HandlerFactory createHandler;
    std::vector<CommandEntry> subcommands;
};

}  // namespace scrap::Command
