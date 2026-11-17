#include "command/ToolchainListingTable.h"

#include "command/PrintableText.h"
#include "command/TextColumns.h"
#include "command/ToolchainListing.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * What @p toolchain lacks, to follow a mark on it.
 */
std::string_view absence(const ListedToolchain& toolchain)
{
    return toolchain.isSystem ? ", not found" : ", not installed";
}

std::string nameCell(const ListedToolchain& toolchain)
{
    std::string cell = printableText(toolchain.name);
    if (toolchain.compiler.has_value()) {
        cell += " (" + printablePath(*toolchain.compiler) + ")";
    }
    return cell;
}

std::string defaultCell(const ListedToolchain& toolchain)
{
    if (! toolchain.isDefault) {
        return {};
    }
    std::string cell = "yes";
    if (! toolchain.present) {
        cell += absence(toolchain);
    }
    return cell;
}

std::string usedHereCell(const ListedToolchain& toolchain)
{
    if (! toolchain.usedHere.has_value()) {
        return {};
    }
    std::string cell{ describeUseReason(*toolchain.usedHere) };
    if (! toolchain.present) {
        cell += absence(toolchain);
    }
    return cell;
}

}  // anonymous namespace

std::string renderToolchainListingTable(const ToolchainListing& listing)
{
    std::vector<std::vector<std::string>> rows;
    rows.push_back({ "TOOLCHAIN", "DEFAULT" });
    if (listing.inProject) {
        rows.back().emplace_back("USED HERE");
    }
    for (const ListedToolchain& toolchain : listing.toolchains) {
        rows.push_back({ nameCell(toolchain), defaultCell(toolchain) });
        if (listing.inProject) {
            rows.back().push_back(usedHereCell(toolchain));
        }
    }
    return layOutColumns(rows);
}

}  // namespace scrap::Command
