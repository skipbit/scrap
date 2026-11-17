#include "command/ToolchainListing.h"

#include "command/ToolchainNaming.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/SystemCompiler.h"
#include "toolchain/ToolchainChoice.h"
#include "toolchain/ToolchainSpecifier.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

constexpr std::string_view SystemName = "system";

/**
 * The name of the newest installed toolchain @p specifier names, if any.
 */
std::optional<std::string> newestNamed(const std::vector<Toolchain::InstalledName>& installed, const Toolchain::ToolchainSpecifier& specifier)
{
    const Toolchain::InstalledName* newest = nullptr;
    for (const Toolchain::InstalledName& each : installed) {
        if (specifier.matches(each.family, each.version) && ((newest == nullptr) || (each.version > newest->version))) {
            newest = &each;
        }
    }
    if (newest == nullptr) {
        return std::nullopt;
    }
    return newest->text();
}

/**
 * The row of @p toolchains that @p named stands for, added at the end, as
 * not there, when there is none.
 */
ListedToolchain& rowFor(std::vector<ListedToolchain>& toolchains, const std::vector<Toolchain::InstalledName>& installed, const NamedToolchain& named)
{
    const bool isSystem = std::holds_alternative<Toolchain::SystemToolchain>(named.choice);
    std::optional<std::string> present;
    if (isSystem) {
        present = std::string{ SystemName };
    } else {
        present = newestNamed(installed, std::get<Toolchain::ToolchainSpecifier>(named.choice));
    }
    const std::string& name = present.value_or(named.text);
    const auto found = std::ranges::find(toolchains, name, &ListedToolchain::name);
    if (found != toolchains.end()) {
        return *found;
    }
    toolchains.push_back(ListedToolchain{ .name = name, .isSystem = isSystem, .compiler = std::nullopt, .present = false, .isDefault = false, .usedHere = std::nullopt });
    return toolchains.back();
}

/**
 * Mark the toolchain @p here names as the one used here.
 */
void markUsedHere(std::vector<ListedToolchain>& toolchains, const ListingFacts& facts, const NamedHere& here)
{
    if (const auto* named = std::get_if<NamedToolchain>(&here)) {
        rowFor(toolchains, facts.installed, *named).usedHere = named->source;
        return;
    }
    if (std::holds_alternative<NothingNamedHere>(here) && facts.systemCompiler.has_value()) {
        toolchains.front().usedHere = facts.systemCompiler->origin;
    }
}

std::string_view describe(const ChoiceSource source)
{
    return describeSource(source);
}

std::string_view describe(const Toolchain::CompilerOrigin origin)
{
    return describeOrigin(origin);
}

}  // anonymous namespace

std::string_view describeUseReason(const UseReason& reason)
{
    return std::visit([](const auto& alternative) -> std::string_view {
        return describe(alternative);
    }, reason);
}

ToolchainListing listToolchains(const ListingFacts& facts)
{
    std::vector<ListedToolchain> toolchains;
    if (facts.systemCompiler.has_value()) {
        toolchains.push_back(ListedToolchain{ .name = std::string{ SystemName }, .isSystem = true, .compiler = facts.systemCompiler->path, .present = true, .isDefault = false, .usedHere = std::nullopt });
    }
    for (const Toolchain::InstalledName& each : facts.installed) {
        toolchains.push_back(ListedToolchain{ .name = each.text(), .isSystem = false, .compiler = std::nullopt, .present = true, .isDefault = false, .usedHere = std::nullopt });
    }
    if (facts.globalDefault.has_value()) {
        rowFor(toolchains, facts.installed, *facts.globalDefault).isDefault = true;
    }
    if (facts.here.has_value()) {
        markUsedHere(toolchains, facts, *facts.here);
    }
    return ToolchainListing{ .toolchains = std::move(toolchains), .inProject = facts.here.has_value() };
}

}  // namespace scrap::Command
