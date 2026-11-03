#include "toolchain/IngotSelection.h"

#include "toolchain/HostEnvironment.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <algorithm>
#include <expected>  // IWYU pragma: keep
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace scrap::Toolchain {

namespace {

const Ingot* ingotFor(const IndexedToolchain& toolchain, const HostEnvironment& environment)
{
    const auto found = std::ranges::find_if(toolchain.ingots, [&environment](const Ingot& ingot) {
        return (ingot.platform == environment.platform) && (ingot.arch == environment.arch);
    });
    return (found == toolchain.ingots.end()) ? nullptr : &*found;
}

/**
 * The toolchains of @p family that @p accepts takes, newest first.
 */
template <typename Predicate>
std::vector<const IndexedToolchain*> newestFirst(const ToolchainIndex& index, const std::string& family, Predicate accepts)
{
    std::vector<const IndexedToolchain*> found;
    for (const IndexedToolchain& toolchain : index.toolchains) {
        if ((toolchain.family == family) && accepts(toolchain)) {
            found.push_back(&toolchain);
        }
    }
    std::ranges::stable_sort(found, [](const IndexedToolchain* left, const IndexedToolchain* right) {
        return left->version > right->version;
    });
    return found;
}

std::vector<std::string> versionTexts(const std::vector<const IndexedToolchain*>& toolchains)
{
    std::vector<std::string> texts;
    texts.reserve(toolchains.size());
    for (const IndexedToolchain* toolchain : toolchains) {
        texts.push_back(toolchain->version.text());
    }
    return texts;
}

std::vector<std::string> familyNames(const ToolchainIndex& index)
{
    std::vector<std::string> names;
    for (const IndexedToolchain& toolchain : index.toolchains) {
        names.push_back(toolchain.family);
    }
    std::ranges::sort(names);
    const auto repeated = std::ranges::unique(names);
    names.erase(repeated.begin(), repeated.end());
    return names;
}

bool needsNewerScrap(const IndexedToolchain& toolchain, const InstallingHost& host)
{
    return toolchain.minScrapVersion.has_value() && (*toolchain.minScrapVersion > host.scrapVersion);
}

bool needsNewerGlibc(const Ingot& ingot, const InstallingHost& host)
{
    return ingot.glibcVersion.has_value() && host.glibcVersion.has_value() && (*ingot.glibcVersion > *host.glibcVersion);
}

bool isOffered(const IndexedToolchain& toolchain)
{
    return toolchain.status != ToolchainStatus::Disabled;
}

bool canBeInstalled(const IndexedToolchain& toolchain, const InstallingHost& host)
{
    if (needsNewerScrap(toolchain, host) || (! isOffered(toolchain))) {
        return false;
    }
    const Ingot* ingot = ingotFor(toolchain, host.environment);
    return (ingot != nullptr) && (! needsNewerGlibc(*ingot, host));
}

/**
 * What stops @p toolchain, which cannot be installed on @p host. A toolchain
 * needing a newer scrap comes first, since nothing past that was read of it.
 */
IngotSelectionFault faultOf(const IndexedToolchain& toolchain, const ToolchainIndex& index, const InstallingHost& host)
{
    if (needsNewerScrap(toolchain, host)) {
        return NeedsNewerScrap{ .required = toolchain.minScrapVersion->text() };
    }
    if (! isOffered(toolchain)) {
        const auto installable = newestFirst(index, toolchain.family, [&host](const IndexedToolchain& other) {
            return canBeInstalled(other, host);
        });
        return NoLongerOffered{ .family = toolchain.family,
                                .alternative = installable.empty() ? std::nullopt : std::optional{ installable.front()->version.text() } };
    }
    const Ingot* ingot = ingotFor(toolchain, host.environment);
    if (ingot == nullptr) {
        return NoIngotForHost{ .versions = versionTexts(newestFirst(index, toolchain.family, [&host](const IndexedToolchain& other) {
            return isOffered(other) && (! needsNewerScrap(other, host)) && (ingotFor(other, host.environment) != nullptr);
        })) };
    }
    return NeedsNewerGlibc{ .required = ingot->glibcVersion->text(), .host = host.glibcVersion->text() };
}

}  // anonymous namespace

std::expected<SelectedIngot, IngotSelectionFault> selectIngot(const ToolchainIndex& index, const ToolchainSpecifier& specifier, const InstallingHost& host)
{
    const auto named = newestFirst(index, specifier.family, [&specifier](const IndexedToolchain& toolchain) {
        return specifier.matches(toolchain.family, toolchain.version);
    });
    if (named.empty()) {
        return std::unexpected(NoMatchInIndex{ .versions = versionTexts(newestFirst(index, specifier.family, isOffered)), .families = familyNames(index) });
    }
    const auto chosen = std::ranges::find_if(named, [&host](const IndexedToolchain* toolchain) {
        return canBeInstalled(*toolchain, host);
    });
    if (chosen == named.end()) {
        return std::unexpected(faultOf(*named.front(), index, host));
    }
    const IndexedToolchain& toolchain = **chosen;
    return SelectedIngot{ .family = toolchain.family, .version = toolchain.version, .compiler = toolchain.compiler, .ingot = *ingotFor(toolchain, host.environment) };
}

}  // namespace scrap::Toolchain
