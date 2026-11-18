#include "toolchain/IndexChanges.h"

#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainIndex.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace scrap::Toolchain {

namespace {

const IndexedToolchain* findSame(const ToolchainIndex& index, const IndexedToolchain& toolchain)
{
    const auto found = std::ranges::find_if(index.toolchains, [&toolchain](const IndexedToolchain& each) {
        return (each.family == toolchain.family) && (each.version == toolchain.version);
    });
    return (found == index.toolchains.end()) ? nullptr : &*found;
}

std::vector<const IndexedToolchain*> sortedByName(const ToolchainIndex& index)
{
    std::vector<const IndexedToolchain*> sorted;
    sorted.reserve(index.toolchains.size());
    for (const IndexedToolchain& toolchain : index.toolchains) {
        sorted.push_back(&toolchain);
    }
    std::ranges::stable_sort(sorted, [](const IndexedToolchain* left, const IndexedToolchain* right) {
        if (left->family != right->family) {
            return left->family < right->family;
        }
        return left->version < right->version;
    });
    return sorted;
}

}  // anonymous namespace

IndexChanges compareIndexes(const ToolchainIndex& before, const ToolchainIndex& after, const InstallingHost& host)
{
    IndexChanges changes;
    for (const IndexedToolchain* now : sortedByName(after)) {
        const IndexedToolchain* then = findSame(before, *now);
        if ((then != nullptr) && hasIngotFor(*then, host.environment) && hasIngotFor(*now, host.environment) && (then->status != now->status)) {
            changes.changed.push_back(ChangedToolchain{ .name = now->text(), .before = then->status, .after = now->status });
        } else if (canBeInstalled(*now, host) && ((then == nullptr) || (! canBeInstalled(*then, host)))) {
            changes.added.push_back(now->text());
        }
    }
    return changes;
}

std::size_t countInstallable(const ToolchainIndex& index, const InstallingHost& host)
{
    return static_cast<std::size_t>(std::ranges::count_if(index.toolchains, [&host](const IndexedToolchain& toolchain) {
        return canBeInstalled(toolchain, host);
    }));
}

}  // namespace scrap::Toolchain
