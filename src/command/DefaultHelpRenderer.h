#pragma once

#include "command/HelpRenderer.h"

namespace scrap::Command {

/**
 * Default implementation of HelpRenderer.
 *
 * Renders global help with entries grouped by source and category,
 * and per-command help with usage, description, subcommands, and options.
 */
class DefaultHelpRenderer : public HelpRenderer {
public:
    [[nodiscard]] auto renderGlobal(std::span<const HelpEntry> entries) const -> std::string override;
    [[nodiscard]] auto renderCommand(const CommandSpec& spec) const -> std::string override;
};

}  // namespace scrap::Command
