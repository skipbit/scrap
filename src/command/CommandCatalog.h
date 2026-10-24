#pragma once

#include "command/CommandEntry.h"
#include "command/HelpRenderer.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace scrap::Command {

/**
 * @brief Aggregates CommandEntry trees from all Resolvers.
 *
 * Provides priority-ordered lookup by command path,
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
    void addEntries(std::vector<CommandEntry> entries);

    /// What a path of words names: the deepest command they reach and how many words named one.
    struct PathMatch {
        const CommandEntry* deepest = nullptr;
        std::size_t depth = 0;
    };

    /**
     * @brief Walk the tree by a path of command names.
     *
     * Each word is matched exactly against the commands of the
     * current level; the walk stops at the first word that names no
     * command.
     *
     * @param words Command names from the top level down.
     * @return The deepest command reached and the number of words
     *         that named one; @c deepest is @c nullptr when no word did.
     */
    [[nodiscard]] PathMatch match(std::span<const std::string> words) const;

    /**
     * @brief Derive a CommandSpec tree from the stored entries.
     *
     * Each returned CommandSpec has its @c subcommands vector
     * populated recursively from the corresponding CommandEntry
     * subtree.  The CommandEntry invariant (spec.subcommands is
     * always empty) is preserved; only the derived specs carry
     * children.
     */
    [[nodiscard]] std::vector<CommandSpec> specs() const;

    /**
     * @brief Derive a flat HelpEntry list for HelpRenderer.
     *
     * Each HelpEntry pairs a fully-populated CommandSpec (with
     * recursive subcommands) and the originating CommandSource,
     * so the renderer can group entries by source category.
     */
    [[nodiscard]] std::vector<HelpEntry> helpEntries() const;

    /**
     * @brief Recursively build a CommandSpec from a CommandEntry.
     *
     * Copies spec fields and populates subcommands by walking the
     * entry's subcommand tree depth-first.
     */
    [[nodiscard]] static CommandSpec specOf(const CommandEntry& entry);

private:
    std::vector<CommandEntry> _entries;
};

}  // namespace scrap::Command
