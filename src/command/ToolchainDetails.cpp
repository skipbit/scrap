#include "command/ToolchainDetails.h"

#include "command/ToolchainListing.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <string_view>

namespace scrap::Command {

ToolchainSelection selectionIn(const ToolchainListing& listing, const std::string_view name)
{
    const auto listed = std::ranges::find_if(listing.toolchains, [name](const ListedToolchain& toolchain) {
        return toolchain.present && (toolchain.name == name);
    });
    if (listed == listing.toolchains.end()) {
        return ToolchainSelection{ .isDefault = false, .usedHere = std::nullopt, .inProject = listing.inProject, .hereUnreadable = listing.hereUnreadable };
    }
    return ToolchainSelection{ .isDefault = listed->isDefault, .usedHere = listed->usedHere, .inProject = listing.inProject, .hereUnreadable = listing.hereUnreadable };
}

ToolchainSelection selectionOfNotInstalled(const ToolchainListing& listing, const std::function<bool(std::string_view)>& leadsToIt)
{
    ToolchainSelection selection{ .isDefault = false, .usedHere = std::nullopt, .inProject = listing.inProject, .hereUnreadable = listing.hereUnreadable };
    for (const ListedToolchain& toolchain : listing.toolchains) {
        if (toolchain.present || toolchain.isSystem || (! leadsToIt(toolchain.name))) {
            continue;
        }
        selection.isDefault = selection.isDefault || toolchain.isDefault;
        if (toolchain.usedHere.has_value()) {
            selection.usedHere = toolchain.usedHere;
        }
    }
    return selection;
}

}  // namespace scrap::Command
