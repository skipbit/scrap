#include "command/ToolchainListingTable.h"

#include "command/PrintableText.h"
#include "command/ToolchainListing.h"
#include "command/ToolchainNaming.h"
#include "toolchain/SystemCompiler.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

constexpr std::string_view ColumnGap = "  ";

/**
 * What @p toolchain lacks, to follow a mark on it.
 */
std::string_view absence(const ListedToolchain& toolchain)
{
    return toolchain.name == "system" ? ", not found" : ", not installed";
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
    std::string cell;
    if (const auto* source = std::get_if<ChoiceSource>(&*toolchain.usedHere)) {
        cell = describeSource(*source);
    } else {
        cell = describeOrigin(std::get<Toolchain::CompilerOrigin>(*toolchain.usedHere));
    }
    if (! toolchain.present) {
        cell += absence(toolchain);
    }
    return cell;
}

/**
 * @p rows laid out in columns, each as wide as its widest cell.
 */
std::string layOut(const std::vector<std::vector<std::string>>& rows)
{
    std::vector<std::size_t> widths(rows.front().size(), 0);
    for (const auto& row : rows) {
        for (std::size_t column = 0; column < row.size(); ++column) {
            widths[column] = std::max(widths[column], row[column].size());
        }
    }
    std::string text;
    for (const auto& row : rows) {
        std::string line;
        for (std::size_t column = 0; column < row.size(); ++column) {
            if (column > 0) {
                line += ColumnGap;
            }
            line += row[column];
            line.append(widths[column] - row[column].size(), ' ');
        }
        line.erase(line.find_last_not_of(' ') + 1);
        text += line + "\n";
    }
    return text;
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
    return layOut(rows);
}

}  // namespace scrap::Command
