#pragma once

#include "command/CommandSource.h"
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
    virtual ~HelpRenderer();
    HelpRenderer(const HelpRenderer&) = default;
    HelpRenderer& operator=(const HelpRenderer&) = default;
    HelpRenderer(HelpRenderer&&) = default;
    HelpRenderer& operator=(HelpRenderer&&) = default;

    virtual auto renderGlobal(std::span<const HelpEntry> entries) const -> std::string = 0;
    virtual auto renderCommand(const CommandSpec& spec) const -> std::string = 0;

protected:
    HelpRenderer() = default;
};

}  // namespace scrap::Command
