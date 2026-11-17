#include "command/ToolchainDetailsText.h"

#include "command/PrintableText.h"
#include "command/ToolchainDetails.h"
#include "command/ToolchainListing.h"
#include "command/ToolchainNaming.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/SystemCompiler.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainIndexParser.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

constexpr std::string_view NotRecorded = "not recorded";

/**
 * One line of details. A label left empty continues the value above it.
 */
struct DetailLine {
    std::string label;
    std::string value;
};

DetailLine line(std::string label, std::string value)
{
    return DetailLine{ .label = std::move(label), .value = std::move(value) };
}

std::string layOut(const std::vector<DetailLine>& lines)
{
    std::size_t width = 0;
    for (const DetailLine& line : lines) {
        width = std::max(width, line.label.size());
    }
    std::string text;
    for (const DetailLine& line : lines) {
        const std::string label = line.label.empty() ? std::string{} : line.label + ":";
        text += label + std::string(width + 3 - label.size(), ' ') + line.value + "\n";
    }
    return text;
}

std::string recorded(const std::optional<std::string>& value)
{
    return value.has_value() ? printableText(*value) : std::string{ NotRecorded };
}

std::string joined(const std::vector<std::string>& arguments)
{
    if (arguments.empty()) {
        return "(none)";
    }
    std::string text;
    for (const std::string& argument : arguments) {
        if (! text.empty()) {
            text += ' ';
        }
        text += printableText(argument);
    }
    return text;
}

std::string usedHereText(const UseReason& reason)
{
    if (const auto* source = std::get_if<ChoiceSource>(&reason)) {
        return std::string{ describeSource(*source) };
    }
    return std::string{ describeOrigin(std::get<Toolchain::CompilerOrigin>(reason)) };
}

void addSelection(std::vector<DetailLine>& lines, const ToolchainSelection& selection)
{
    lines.push_back(line("default", selection.isDefault ? "yes" : "no"));
    if (selection.inProject) {
        lines.push_back(line("used here", selection.usedHere.has_value() ? usedHereText(*selection.usedHere) : "no"));
    }
}

std::string reasonText(const Toolchain::NoMatchInIndex& /*barrier*/)
{
    return "not in the toolchain index";
}

std::string reasonText(const Toolchain::NoIngotForHost& barrier)
{
    return "not built for " + printableText(barrier.environment);
}

std::string reasonText(const Toolchain::NoLongerOffered& /*barrier*/)
{
    return "no longer provided";
}

std::string reasonText(const Toolchain::NeedsNewerGlibc& barrier)
{
    return "needs glibc " + printableText(barrier.required) + " or later; this host has " + printableText(barrier.host);
}

std::string reasonText(const Toolchain::NeedsNewerScrap& barrier)
{
    return "needs scrap " + printableText(barrier.required) + " or later";
}

/**
 * Why a toolchain the index holds cannot be installed on the host, as
 * install tells it.
 */
std::string barrierText(const Toolchain::IngotSelectionFault& barrier)
{
    return std::visit([](const auto& alternative) -> std::string {
        return reasonText(alternative);
    }, barrier);
}

std::vector<DetailLine> linesOf(const InstalledDetails& details)
{
    const Toolchain::InstallRecord& record = details.record;
    std::vector<DetailLine> lines{
        line("name", printableText(details.name)),
        line("installed", "yes"),
        line("directory", printablePath(details.directory)),
        line("source", recorded(record.source)),
        line("integrity", recorded(record.ingotIntegrity)),
        line("installed at", recorded(record.installedAt)),
        line("compiler", printablePath(details.directory / record.compiler)),
    };
    if (record.cCompiler.has_value()) {
        lines.push_back(line("C compiler", printablePath(details.directory / *record.cCompiler)));
    }
    if (record.runtimeLibraryDirectories.empty()) {
        lines.push_back(line("runtime libraries", "(none)"));
    }
    for (std::size_t i = 0; i < record.runtimeLibraryDirectories.size(); ++i) {
        lines.push_back(line((i == 0) ? "runtime libraries" : "", printablePath(details.directory / record.runtimeLibraryDirectories[i])));
    }
    lines.push_back(line("compile arguments", joined(record.arguments.compile)));
    lines.push_back(line("link arguments", joined(record.arguments.link)));
    addSelection(lines, details.selection);
    return lines;
}

std::vector<DetailLine> linesOf(const IndexedDetails& details)
{
    const Toolchain::IndexedToolchain& toolchain = details.lookup.toolchain;
    std::vector<DetailLine> lines{
        line("name", printableText(toolchain.family + "@" + toolchain.version.text())),
        line("installed", "no"),
        line("status", std::string{ Toolchain::statusName(toolchain.status) }),
    };
    if (const std::optional<Toolchain::IngotSelectionFault>& barrier = details.lookup.barrier; barrier.has_value()) {
        lines.push_back(line("cannot be installed here", barrierText(*barrier)));
    }
    if (! toolchain.compiler.empty()) {
        lines.push_back(line("compiler", printableText(toolchain.compiler)));
    }
    if (toolchain.cCompiler.has_value()) {
        lines.push_back(line("C compiler", printableText(*toolchain.cCompiler)));
    }
    if (const std::optional<Toolchain::Ingot>& ingot = details.lookup.ingot; ingot.has_value()) {
        lines.push_back(line("source", printableText(Toolchain::sourceOf(*ingot))));
        lines.push_back(line("integrity", printableText(Toolchain::integrityOf(*ingot))));
    }
    lines.push_back(line("index fetched at", details.fetchedAt.has_value() ? printableText(*details.fetchedAt) : "unknown"));
    return lines;
}

std::vector<DetailLine> linesOf(const SystemDetails& details)
{
    std::vector<DetailLine> lines{
        line("name", "system"),
        line("found", "yes"),
        line("compiler", printablePath(details.compiler.path) + " (" + std::string{ describeOrigin(details.compiler.origin) } + ")"),
    };
    addSelection(lines, details.selection);
    return lines;
}

}  // anonymous namespace

std::string renderToolchainDetails(const ToolchainDetails& details)
{
    return layOut(std::visit([](const auto& alternative) {
        return linesOf(alternative);
    }, details));
}

std::optional<std::string> renderUnrecordedHint(const InstalledDetails& details)
{
    if (details.record.source.has_value() && details.record.installedAt.has_value()) {
        return std::nullopt;
    }
    return "hint: reinstall it with 'scrap toolchain install --force " + printableText(details.name) + "' to record them\n";
}

}  // namespace scrap::Command
