#pragma once

#include "command/CommandEntry.h"
#include "command/HelpRenderer.h"

#include <string>
#include <vector>

namespace scrap::Command {

/**
 * @brief Aggregates CommandEntry trees from all Resolvers.
 *
 * Provides priority-ordered lookup by dot-separated command path,
 * and derives read-only CommandSpec / HelpEntry views from the
 * stored CommandEntry tree.
 *
 * Entries are added via addEntries(); the first registration for a
 * given top-level name wins, and subsequent duplicates are dropped
 * with a warning.
 */
class CommandCatalog {
public:
    CommandCatalog();
    ~CommandCatalog();

    CommandCatalog(const CommandCatalog&) = delete;
    CommandCatalog& operator=(const CommandCatalog&) = delete;
    CommandCatalog(CommandCatalog&&) noexcept;
    CommandCatalog& operator=(CommandCatalog&&) noexcept;

    /**
     * @brief Merge entries from a single resolver into the catalog.
     *
     * For each incoming entry, if an entry with the same top-level
     * name already exists the incoming one is silently dropped and a
     * warning is written to stderr.  Call order determines priority:
     * the first resolver to register a name wins.
     *
     * @param entries CommandEntry trees to merge.
     */
    auto addEntries(std::vector<CommandEntry> entries) -> void;

    /**
     * @brief Look up a CommandEntry by dot-separated command path.
     *
     * The path is split on @c '.' and each segment is matched
     * against the corresponding tree level.  Malformed paths
     * (leading/trailing dots, consecutive dots) are rejected.
     *
     * @param commandPath Dot-separated path, e.g. "toolchain.install".
     * @return Pointer to the found entry, or @c nullptr if not found.
     */
    [[nodiscard]] auto find(const std::string& commandPath) const -> const CommandEntry*;

    /**
     * @brief Derive a CommandSpec tree from the stored entries.
     *
     * Each returned CommandSpec has its @c subcommands vector
     * populated recursively from the corresponding CommandEntry
     * subtree.  The CommandEntry invariant (spec.subcommands is
     * always empty) is preserved; only the derived specs carry
     * children.
     */
    [[nodiscard]] auto specs() const -> std::vector<CommandSpec>;

    /**
     * @brief Derive a flat HelpEntry list for HelpRenderer.
     *
     * Each HelpEntry pairs a fully-populated CommandSpec (with
     * recursive subcommands) and the originating CommandSource,
     * so the renderer can group entries by source category.
     */
    [[nodiscard]] auto helpEntries() const -> std::vector<HelpEntry>;

private:
    std::vector<CommandEntry> entries_;

    /**
     * @brief Recursively build a CommandSpec from a CommandEntry.
     *
     * Copies spec fields and populates subcommands by walking the
     * entry's subcommand tree depth-first.
     */
    static auto buildSpec(const CommandEntry& entry) -> CommandSpec;
};

}  // namespace scrap::Command
