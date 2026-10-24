#include "command/CommandCatalog.h"

#include "command/CommandEntry.h"
#include "command/CommandSpec.h"
#include "command/HelpRenderer.h"

#include <algorithm>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace scrap::Command {

// --- Special members ----------------------------------------------------------

CommandCatalog::CommandCatalog() = default;
CommandCatalog::~CommandCatalog() = default;
CommandCatalog::CommandCatalog(CommandCatalog&&) noexcept = default;
CommandCatalog& CommandCatalog::operator=(CommandCatalog&&) noexcept = default;

// --- Public interface ---------------------------------------------------------

void CommandCatalog::addEntries(std::vector<CommandEntry> entries)
{
    for (auto& incoming : entries) {
        const auto i = std::ranges::find_if(_entries, [&](const CommandEntry& existing) {
            return existing.spec.name == incoming.spec.name;
        });

        if (i != _entries.end()) {
            std::cerr << "warning: command '" << incoming.spec.name << "' already registered; ignoring duplicate\n";
            continue;
        }

        _entries.push_back(std::move(incoming));
    }
}

CommandCatalog::PathMatch CommandCatalog::match(std::span<const std::string> words) const
{
    PathMatch result;
    const std::vector<CommandEntry>* level = &_entries;
    for (const auto& word : words) {
        const auto it = std::ranges::find_if(*level, [&](const CommandEntry& entry) {
            return entry.spec.name == word;
        });
        if (it == level->end()) {
            break;
        }
        result.deepest = &*it;
        ++result.depth;
        level = &it->subcommands;
    }
    return result;
}

std::vector<CommandSpec> CommandCatalog::specs() const
{
    std::vector<CommandSpec> result;
    result.reserve(_entries.size());

    for (const auto& entry : _entries) {
        result.push_back(specOf(entry));
    }

    return result;
}

std::vector<HelpEntry> CommandCatalog::helpEntries() const
{
    std::vector<HelpEntry> result;
    result.reserve(_entries.size());

    for (const auto& entry : _entries) {
        result.push_back(HelpEntry{ specOf(entry), entry.source });
    }

    return result;
}

// NOLINTNEXTLINE(readability-function-size) - iterative BFS requires local struct + loop state
CommandSpec CommandCatalog::specOf(const CommandEntry& entry)
{
    CommandSpec root = entry.spec;
    root.subcommands.clear();

    // Iterative BFS: process each tree level without recursion.
    struct Pending {
        const std::vector<CommandEntry>* sources;
        CommandSpec* dest;
    };

    std::vector<Pending> current;
    if (! entry.subcommands.empty()) {
        current.push_back({ &entry.subcommands, &root });
    }

    while (! current.empty()) {
        std::vector<Pending> next;
        for (auto& [sources, dest] : current) {
            dest->subcommands.reserve(sources->size());
            for (const auto& child : *sources) {
                auto& added = dest->subcommands.emplace_back(child.spec);
                added.subcommands.clear();
                if (! child.subcommands.empty()) {
                    next.push_back({ &child.subcommands, &added });
                }
            }
        }
        current = std::move(next);
    }

    return root;
}

}  // namespace scrap::Command
