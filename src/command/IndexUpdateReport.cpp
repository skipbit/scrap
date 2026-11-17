#include "command/IndexUpdateReport.h"

#include "toolchain/IndexChanges.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainIndexParser.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace scrap::Command {

namespace {

std::string fetchedLine(std::string_view fetchedAt)
{
    return "fetched the toolchain index at " + std::string{ fetchedAt };
}

/**
 * Each changed toolchain on a line of its own, its statuses lined up after
 * the longest name.
 */
std::string changedLines(const Toolchain::IndexChanges& changes)
{
    std::size_t width = 0;
    for (const Toolchain::ChangedToolchain& each : changes.changed) {
        width = std::max(width, each.name.size());
    }
    std::string text = "changed:\n";
    for (const Toolchain::ChangedToolchain& each : changes.changed) {
        text += "  " + each.name + std::string(width - each.name.size() + 2, ' ') + std::string{ Toolchain::statusName(each.before) } + " -> " + std::string{ Toolchain::statusName(each.after) } + "\n";
    }
    return text;
}

}  // anonymous namespace

std::string renderIndexChanges(std::string_view fetchedAt, std::string_view hostName, const Toolchain::IndexChanges& changes)
{
    if (changes.added.empty() && changes.changed.empty()) {
        return fetchedLine(fetchedAt) + "; nothing has changed for " + std::string{ hostName } + "\n";
    }
    std::string text = fetchedLine(fetchedAt) + "\n";
    if (! changes.added.empty()) {
        text += "new:\n";
        for (const std::string& name : changes.added) {
            text += "  " + name + "\n";
        }
    }
    if (! changes.changed.empty()) {
        text += changedLines(changes);
    }
    return text;
}

std::string renderFirstIndex(std::string_view fetchedAt, std::string_view hostName, const std::size_t installable)
{
    const std::string count = std::to_string(installable) + ((installable == 1) ? " toolchain" : " toolchains");
    return fetchedLine(fetchedAt) + ": " + count + " for " + std::string{ hostName } + "\n";
}

}  // namespace scrap::Command
