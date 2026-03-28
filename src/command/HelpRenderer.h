#pragma once

#include "command/CommandEntry.h"
#include "command/CommandSpec.h"

#include <span>
#include <string>

namespace scrap::Command {

struct HelpEntry {
    CommandSpec spec;
    CommandSource source;
};

class HelpRenderer {
public:
    virtual ~HelpRenderer() = default;
    virtual auto renderGlobal(std::span<const HelpEntry> entries) const -> std::string = 0;
    virtual auto renderCommand(const CommandSpec& spec) const -> std::string = 0;
};

}  // namespace scrap::Command
