#include "command/CommandCatalog.h"

#include "command/CommandEntry.h"
#include "command/CommandSpec.h"
#include "command/HelpRenderer.h"

#include <algorithm>
#include <iostream>
#include <sstream>
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

const CommandEntry* CommandCatalog::find(const std::string& commandPath) const
{
    if (commandPath.empty()) {
        return nullptr;
    }

    // Reject malformed paths: leading/trailing dots, consecutive dots.
    if ((commandPath.front() == '.') || (commandPath.back() == '.') || commandPath.contains("..")) {
        return nullptr;
    }

    // Split path on '.'.
    std::vector<std::string> segments;
    std::istringstream stream(commandPath);
    std::string segment;
    while (std::getline(stream, segment, '.')) {
        segments.push_back(std::move(segment));
    }

    if (segments.empty()) {
        return nullptr;
    }

    // Walk the tree level by level.
    const std::vector<CommandEntry>* currentLevel = &_entries;
    const CommandEntry* found = nullptr;

    for (const auto& seg : segments) {
        const auto it = std::ranges::find_if(*currentLevel, [&](const CommandEntry& entry) {
            return entry.spec.name == seg;
        });

        if (it == currentLevel->end()) {
            return nullptr;
        }

        found = &(*it);
        currentLevel = &found->subcommands;
    }

    return found;
}

std::vector<CommandSpec> CommandCatalog::specs() const
{
    std::vector<CommandSpec> result;
    result.reserve(_entries.size());

    for (const auto& entry : _entries) {
        result.push_back(buildSpec(entry));
    }

    return result;
}

std::vector<HelpEntry> CommandCatalog::helpEntries() const
{
    std::vector<HelpEntry> result;
    result.reserve(_entries.size());

    for (const auto& entry : _entries) {
        result.push_back(HelpEntry{ buildSpec(entry), entry.source });
    }

    return result;
}

// --- Private helpers ----------------------------------------------------------

// NOLINTNEXTLINE(readability-function-size) - iterative BFS requires local struct + loop state
CommandSpec CommandCatalog::buildSpec(const CommandEntry& entry)
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
