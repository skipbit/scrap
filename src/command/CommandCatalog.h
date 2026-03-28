#pragma once

#include "command/CommandEntry.h"
#include "command/HelpRenderer.h"

#include <string>
#include <vector>

namespace scrap::Command {

/// @brief Aggregates CommandEntry trees from all Resolvers, provides
///        priority-ordered lookup, and derives CommandSpec / HelpEntry views.
class CommandCatalog {
public:
    CommandCatalog();
    ~CommandCatalog();

    CommandCatalog(const CommandCatalog&) = delete;
    CommandCatalog& operator=(const CommandCatalog&) = delete;
    CommandCatalog(CommandCatalog&&) noexcept;
    CommandCatalog& operator=(CommandCatalog&&) noexcept;

    /// @brief Add entries from a resolver.
    ///        First-registered name wins on collision; a warning is emitted
    ///        for every duplicate top-level name that is silently dropped.
    /// @param entries CommandEntry trees to merge into the catalog.
    auto addEntries(std::vector<CommandEntry> entries) -> void;

    /// @brief Look up a CommandEntry by dot-separated path.
    /// @param commandPath Dot-separated command path (e.g. "toolchain.install").
    /// @return Pointer to the found entry, or nullptr if not found.
    [[nodiscard]] auto find(const std::string& commandPath) const -> const CommandEntry*;

    /// @brief Derive a CommandSpec tree from the stored CommandEntry tree.
    ///        Each returned CommandSpec has its subcommands vector populated
    ///        recursively from the corresponding CommandEntry subtree.
    [[nodiscard]] auto specs() const -> std::vector<CommandSpec>;

    /// @brief Derive a flat HelpEntry list suitable for HelpRenderer.
    ///        Each HelpEntry contains a fully-populated CommandSpec (with
    ///        subcommands) and the originating CommandSource.
    [[nodiscard]] auto helpEntries() const -> std::vector<HelpEntry>;

private:
    std::vector<CommandEntry> entries_;

    /// @brief Recursively build a CommandSpec tree from a CommandEntry.
    static auto buildSpec(const CommandEntry& entry) -> CommandSpec;
};

}  // namespace scrap::Command
