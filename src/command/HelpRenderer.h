#pragma once

#include "command/CommandSource.h"
#include "command/CommandSpec.h"

#include <span>
#include <string>
#include <string_view>

namespace scrap::Command {

struct HelpEntry {
    CommandSpec spec;
    CommandSource source = CommandSource::Builtin;
};

class HelpRenderer {
public:
    virtual ~HelpRenderer();
    HelpRenderer(const HelpRenderer&) = default;
    HelpRenderer& operator=(const HelpRenderer&) = default;
    HelpRenderer(HelpRenderer&&) = default;
    HelpRenderer& operator=(HelpRenderer&&) = default;

    [[nodiscard]] virtual std::string renderGlobal(std::span<const HelpEntry> entries) const = 0;
    /**
     * Render the help of @p spec, the command the user names by @p path, its
     * words separated by spaces (e.g. "toolchain list").
     */
    [[nodiscard]] virtual std::string renderCommand(std::string_view path, const CommandSpec& spec) const = 0;

protected:
    HelpRenderer() = default;
};

}  // namespace scrap::Command
