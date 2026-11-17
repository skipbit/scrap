#include "command/ToolchainDetails.h"

#include "command/ToolchainListing.h"

#include <algorithm>
#include <optional>
#include <string_view>

namespace scrap::Command {

ToolchainSelection selectionIn(const ToolchainListing& listing, const std::string_view name)
{
    const auto listed = std::ranges::find_if(listing.toolchains, [name](const ListedToolchain& toolchain) {
        return toolchain.present && (toolchain.name == name);
    });
    if (listed == listing.toolchains.end()) {
        return ToolchainSelection{ .isDefault = false, .usedHere = std::nullopt, .inProject = listing.inProject };
    }
    return ToolchainSelection{ .isDefault = listed->isDefault, .usedHere = listed->usedHere, .inProject = listing.inProject };
}

}  // namespace scrap::Command
