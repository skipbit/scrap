#pragma once

#include "command/HelpRenderer.h"

namespace scrap::Command {

/**
 * @brief Default implementation of HelpRenderer.
 *
 * Renders global help with entries grouped by source and category,
 * and per-command help with usage, description, subcommands, and options.
 */
class DefaultHelpRenderer : public HelpRenderer {
public:
    /**
     * @brief Render the top-level help listing all commands.
     *
     * Entries are grouped by CommandSource: Builtin entries are further
     * grouped by category, followed by External and Project sections.
     * Columns are aligned by the longest command name.
     *
     * @param entries HelpEntry list (typically from CommandCatalog::helpEntries()).
     * @return Formatted help string with USAGE header and footer.
     */
    [[nodiscard]] auto renderGlobal(std::span<const HelpEntry> entries) const -> std::string override;

    /**
     * @brief Render help for a single command.
     *
     * Includes USAGE line (with positionals), description paragraph,
     * SUBCOMMANDS section (if any), and OPTIONS section (if any).
     * Empty sections are omitted entirely.
     *
     * @param spec CommandSpec with subcommands and options populated.
     * @return Formatted command help string.
     */
    [[nodiscard]] auto renderCommand(const CommandSpec& spec) const -> std::string override;
};

}  // namespace scrap::Command
