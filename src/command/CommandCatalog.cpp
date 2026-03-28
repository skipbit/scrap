#include "command/CommandCatalog.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace scrap::Command {

// --- Special members ----------------------------------------------------------

CommandCatalog::CommandCatalog() = default;
CommandCatalog::~CommandCatalog() = default;
CommandCatalog::CommandCatalog(CommandCatalog&&) noexcept = default;
CommandCatalog& CommandCatalog::operator=(CommandCatalog&&) noexcept = default;

// --- Public interface ---------------------------------------------------------

auto CommandCatalog::addEntries(std::vector<CommandEntry> entries) -> void
{
    for (auto& incoming : entries) {
        auto it = std::ranges::find_if(
            entries_, [&](const CommandEntry& existing) { return existing.spec.name == incoming.spec.name; });

        if (it != entries_.end()) {
            std::cerr << "warning: command '" << incoming.spec.name << "' already registered; ignoring duplicate\n";
            continue;
        }

        entries_.push_back(std::move(incoming));
    }
}

auto CommandCatalog::find(const std::string& commandPath) const -> const CommandEntry*
{
    if (commandPath.empty()) {
        return nullptr;
    }

    // Reject malformed paths: leading/trailing dots, consecutive dots.
    if (commandPath.front() == '.' || commandPath.back() == '.' || commandPath.contains("..")) {
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
    const std::vector<CommandEntry>* currentLevel = &entries_;
    const CommandEntry* found = nullptr;

    for (const auto& seg : segments) {
        auto it =
            std::ranges::find_if(*currentLevel, [&](const CommandEntry& entry) { return entry.spec.name == seg; });

        if (it == currentLevel->end()) {
            return nullptr;
        }

        found = &(*it);
        currentLevel = &found->subcommands;
    }

    return found;
}

auto CommandCatalog::specs() const -> std::vector<CommandSpec>
{
    std::vector<CommandSpec> result;
    result.reserve(entries_.size());

    for (const auto& entry : entries_) {
        result.push_back(buildSpec(entry));
    }

    return result;
}

auto CommandCatalog::helpEntries() const -> std::vector<HelpEntry>
{
    std::vector<HelpEntry> result;
    result.reserve(entries_.size());

    for (const auto& entry : entries_) {
        result.push_back(HelpEntry{buildSpec(entry), entry.source});
    }

    return result;
}

// --- Private helpers ----------------------------------------------------------

auto CommandCatalog::buildSpec(const CommandEntry& entry) -> CommandSpec
{
    CommandSpec spec = entry.spec;

    // Populate subcommands from the entry tree (entry.spec.subcommands
    // is always empty per the CommandEntry invariant).
    spec.subcommands.clear();
    spec.subcommands.reserve(entry.subcommands.size());

    for (const auto& child : entry.subcommands) {
        spec.subcommands.push_back(buildSpec(child));
    }

    return spec;
}

}  // namespace scrap::Command
