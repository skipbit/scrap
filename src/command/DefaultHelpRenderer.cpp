#include "command/DefaultHelpRenderer.h"

#include "command/CommandSource.h"
#include "command/CommandSpec.h"
#include "command/HelpRenderer.h"
#include "command/OptionSchema.h"
#include "command/PrintableText.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Command {

namespace {

/// The newline a section starts with. clang-format breaks a stream after a
/// leading '\n' literal; named, it stays on the line it belongs to.
constexpr char Newline = '\n';

/**
 * A line of a listing. An external command is named by its filename and
 * describes itself, so both are kept as they are shown.
 */
struct ListingLine {
    std::string name;
    std::string description;
};

/**
 * @p name and @p description as a listing shows them.
 */
ListingLine listingLine(const std::string_view name, const std::string_view description)
{
    return { .name = printableText(name), .description = printableText(description) };
}

/**
 * The width the names of @p lines take, in bytes of the names as they are
 * shown, so that every name fits the column it is padded to.
 */
std::size_t columnWidth(std::span<const ListingLine> lines)
{
    std::size_t width = 0;
    for (const auto& line : lines) {
        width = std::max(width, line.name.size());
    }
    return width;
}

/**
 * Format a single command line with aligned columns.
 *
 * Output: "    <name>    <description>\n"
 * The name is padded to @p width for alignment.
 */
void appendCommandLine(std::ostringstream& out, const ListingLine& line, std::size_t width)
{
    out << "    " << line.name;
    if (! line.description.empty()) {
        auto padding = (width - line.name.size() + 4);
        for (std::size_t i = 0; i < padding; ++i) {
            out << ' ';
        }
        out << line.description;
    }
    out << '\n';
}

/**
 * Append each of @p lines, padded to @p width.
 */
void appendListing(std::ostringstream& out, std::span<const ListingLine> lines, std::size_t width)
{
    for (const auto& line : lines) {
        appendCommandLine(out, line, width);
    }
}

/**
 * Append a section header line (e.g. "Project Commands:").
 */
void appendSectionHeader(std::ostringstream& out, const std::string& title)
{
    out << Newline << title << ":\n";
}

/**
 * Build a USAGE line including positional arguments.
 */
void appendUsageLine(std::ostringstream& out, std::string_view path, const CommandSpec& spec)
{
    out << "USAGE: scrap " << printableText(path);
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
        if (positional.repeats) {
            out << "...";
        }
    }
    if (spec.options.trailing.has_value()) {
        out << " [-- <" << spec.options.trailing->name << ">...]";
    }
    out << '\n';
}

}  // namespace

/**
 * Render the top-level help listing all commands grouped by source.
 */
std::string DefaultHelpRenderer::renderGlobal(std::span<const HelpEntry> entries) const
{
    std::ostringstream out;
    out << "USAGE: scrap [OPTIONS] <COMMAND>\n";

    if (entries.empty()) {
        out << "\nSee 'scrap help <command>' for more information.\n";
        return out.str();
    }

    // Partition entries by source, every column one width across sections.
    std::map<std::string, std::vector<ListingLine>> builtinCategories;
    std::vector<ListingLine> externalLines;
    std::vector<ListingLine> projectLines;
    std::vector<ListingLine> allLines;
    allLines.reserve(entries.size());

    for (const auto& entry : entries) {
        ListingLine line = listingLine(entry.spec.name, entry.spec.description);
        allLines.push_back(line);
        switch (entry.source) {
        case CommandSource::Builtin:
            builtinCategories[entry.spec.category].push_back(std::move(line));
            break;
        case CommandSource::External:
            externalLines.push_back(std::move(line));
            break;
        case CommandSource::Project:
            projectLines.push_back(std::move(line));
            break;
        }
    }
    const std::size_t width = columnWidth(allLines);

    // Builtin entries grouped by category.
    for (const auto& [category, categoryLines] : builtinCategories) {
        if (category.empty()) {
            appendSectionHeader(out, "Commands");
        } else {
            appendSectionHeader(out, category);
        }
        appendListing(out, categoryLines, width);
    }

    // External entries.
    if (! externalLines.empty()) {
        appendSectionHeader(out, "External Commands");
        appendListing(out, externalLines, width);
    }

    // Project entries.
    if (! projectLines.empty()) {
        appendSectionHeader(out, "Project Commands");
        appendListing(out, projectLines, width);
    }

    out << "\nSee 'scrap help <command>' for more information.\n";
    return out.str();
}

/**
 * Render help for a single command with usage, subcommands, and options.
 */
std::string DefaultHelpRenderer::renderCommand(std::string_view path, const CommandSpec& spec) const
{
    std::ostringstream out;

    appendUsageLine(out, path, spec);

    if (! spec.description.empty()) {
        out << Newline << printableText(spec.description) << '\n';
    }

    // Subcommands section.
    if (! spec.subcommands.empty()) {
        out << "\nSUBCOMMANDS:\n";
        std::vector<ListingLine> lines;
        lines.reserve(spec.subcommands.size());
        for (const auto& sub : spec.subcommands) {
            lines.push_back(listingLine(sub.name, sub.description));
        }
        appendListing(out, lines, columnWidth(lines));
    }

    // Options section.
    if (! spec.options.named.empty()) {
        out << "\nOPTIONS:\n";
        std::vector<ListingLine> lines;
        lines.reserve(spec.options.named.size());
        for (const auto& opt : spec.options.named) {
            std::ostringstream optName;
            if (opt.shortName.has_value()) {
                optName << '-' << opt.shortName.value() << ", ";
            }
            optName << "--" << opt.longName;
            lines.push_back(listingLine(optName.str(), opt.description));
        }
        appendListing(out, lines, columnWidth(lines));
    }

    return out.str();
}

}  // namespace scrap::Command
