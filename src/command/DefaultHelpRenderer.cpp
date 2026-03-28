#include "command/DefaultHelpRenderer.h"

#include <algorithm>
#include <map>
#include <sstream>

namespace scrap::Command {

namespace {

/**
 * Compute the maximum command name length for column alignment.
 */
auto maxNameLength(std::span<const HelpEntry> entries) -> std::size_t
{
    std::size_t maxLen = 0;
    for (const auto& entry : entries) {
        maxLen = std::max(maxLen, entry.spec.name.size());
    }
    return maxLen;
}

/**
 * Format a single command line with aligned columns.
 *
 * Output: "    <name>    <description>\n"
 * The name is padded to @p columnWidth for alignment.
 */
void appendCommandLine(std::ostringstream& out,
                       const std::string& name,
                       const std::string& description,
                       std::size_t columnWidth)
{
    out << "    " << name;
    if (! description.empty()) {
        auto padding = columnWidth - name.size() + 4;
        for (std::size_t i = 0; i < padding; ++i) {
            out << ' ';
        }
        out << description;
    }
    out << '\n';
}

/**
 * Append a section header line (e.g. "Project Commands:").
 */
void appendSectionHeader(std::ostringstream& out, const std::string& title)
{
    out << '\n' << title << ":\n";
}

/**
 * Build a USAGE line including positional arguments.
 */
void appendUsageLine(std::ostringstream& out, const CommandSpec& spec)
{
    out << "USAGE: scrap " << spec.name;
    if (! spec.options.named.empty()) {
        out << " [OPTIONS]";
    }
    if (! spec.subcommands.empty()) {
        out << " <COMMAND>";
    }
    for (const auto& positional : spec.options.positional) {
        if (positional.required) {
            out << " <" << positional.name << ">";
        } else {
            out << " [" << positional.name << "]";
        }
    }
    out << '\n';
}

}  // namespace

/**
 * Render the top-level help listing all commands grouped by source.
 */
auto DefaultHelpRenderer::renderGlobal(std::span<const HelpEntry> entries) const -> std::string
{
    std::ostringstream out;
    out << "USAGE: scrap [OPTIONS] <COMMAND>\n";

    if (entries.empty()) {
        out << "\nSee 'scrap help <command>' for more information.\n";
        return out.str();
    }

    auto colWidth = maxNameLength(entries);

    // Partition entries by source.
    std::map<std::string, std::vector<const HelpEntry*>> builtinCategories;
    std::vector<const HelpEntry*> externalEntries;
    std::vector<const HelpEntry*> projectEntries;

    for (const auto& entry : entries) {
        switch (entry.source) {
            case CommandSource::Builtin:
                builtinCategories[entry.spec.category].push_back(&entry);
                break;
            case CommandSource::External:
                externalEntries.push_back(&entry);
                break;
            case CommandSource::Project:
                projectEntries.push_back(&entry);
                break;
        }
    }

    // Builtin entries grouped by category.
    for (const auto& [category, categoryEntries] : builtinCategories) {
        if (category.empty()) {
            appendSectionHeader(out, "Commands");
        } else {
            appendSectionHeader(out, category);
        }
        for (const auto* entry : categoryEntries) {
            appendCommandLine(out, entry->spec.name, entry->spec.description, colWidth);
        }
    }

    // External entries.
    if (! externalEntries.empty()) {
        appendSectionHeader(out, "External Commands");
        for (const auto* entry : externalEntries) {
            appendCommandLine(out, entry->spec.name, entry->spec.description, colWidth);
        }
    }

    // Project entries.
    if (! projectEntries.empty()) {
        appendSectionHeader(out, "Project Commands");
        for (const auto* entry : projectEntries) {
            appendCommandLine(out, entry->spec.name, entry->spec.description, colWidth);
        }
    }

    out << "\nSee 'scrap help <command>' for more information.\n";
    return out.str();
}

/**
 * Render help for a single command with usage, subcommands, and options.
 */
auto DefaultHelpRenderer::renderCommand(const CommandSpec& spec) const -> std::string
{
    std::ostringstream out;

    appendUsageLine(out, spec);

    if (! spec.description.empty()) {
        out << '\n' << spec.description << '\n';
    }

    // Subcommands section.
    if (! spec.subcommands.empty()) {
        out << "\nSUBCOMMANDS:\n";
        std::size_t maxLen = 0;
        for (const auto& sub : spec.subcommands) {
            maxLen = std::max(maxLen, sub.name.size());
        }
        for (const auto& sub : spec.subcommands) {
            appendCommandLine(out, sub.name, sub.description, maxLen);
        }
    }

    // Options section.
    if (! spec.options.named.empty()) {
        out << "\nOPTIONS:\n";
        std::size_t maxLen = 0;
        for (const auto& opt : spec.options.named) {
            std::size_t len = 2 + opt.longName.size();
            if (opt.shortName.has_value()) {
                len += 4;
            }
            maxLen = std::max(maxLen, len);
        }
        for (const auto& opt : spec.options.named) {
            std::ostringstream optName;
            if (opt.shortName.has_value()) {
                optName << '-' << opt.shortName.value() << ", ";
            }
            optName << "--" << opt.longName;
            appendCommandLine(out, optName.str(), opt.description, maxLen);
        }
    }

    return out.str();
}

}  // namespace scrap::Command
