#include "command/NameAfterUninstall.h"

#include "command/ToolchainNaming.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ToolchainSpecifier.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace scrap::Command {

std::optional<NameAfterUninstall> nameAfterUninstall(const NamedToolchain& named, const std::vector<Toolchain::InstalledName>& installed, const std::string_view removed)
{
    const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&named.choice);
    if ((specifier == nullptr) || (Toolchain::newestNamedBy(installed, *specifier) != removed)) {
        return std::nullopt;
    }
    std::vector<Toolchain::InstalledName> left;
    std::ranges::copy_if(installed, std::back_inserter(left), [removed](const Toolchain::InstalledName& each) {
        return each.text() != removed;
    });
    if (auto now = Toolchain::newestNamedBy(left, *specifier); now.has_value()) {
        return LeadsElsewhere{ .name = std::move(*now) };
    }
    return LeadsNowhere{};
}

}  // namespace scrap::Command
