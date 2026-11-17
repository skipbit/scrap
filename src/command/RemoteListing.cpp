#include "command/RemoteListing.h"

#include "toolchain/IngotSelection.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ToolchainIndex.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace scrap::Command {

std::vector<RemoteToolchain> listRemoteToolchains(const Toolchain::ToolchainIndex& index,
                                                  const std::vector<Toolchain::InstalledName>& installed,
                                                  const Toolchain::InstallingHost& host,
                                                  const bool all)
{
    std::vector<const Toolchain::IndexedToolchain*> listed;
    listed.reserve(index.toolchains.size());
    for (const Toolchain::IndexedToolchain& toolchain : index.toolchains) {
        listed.push_back(&toolchain);
    }
    std::ranges::sort(listed, [](const Toolchain::IndexedToolchain* left, const Toolchain::IndexedToolchain* right) {
        if (left->family != right->family) {
            return left->family < right->family;
        }
        return left->version > right->version;
    });
    std::vector<RemoteToolchain> rows;
    for (const Toolchain::IndexedToolchain* toolchain : listed) {
        auto barrier = Toolchain::barrierTo(*toolchain, index, host);
        if (barrier.has_value() && (! all)) {
            continue;
        }
        const bool isInstalled = std::ranges::any_of(installed, [toolchain](const Toolchain::InstalledName& each) {
            return (each.family == toolchain->family) && (each.version == toolchain->version);
        });
        rows.push_back(RemoteToolchain{ .name = toolchain->family + "@" + toolchain->version.text(),
                                        .status = toolchain->status,
                                        .installed = isInstalled,
                                        .barrier = std::move(barrier) });
    }
    return rows;
}

}  // namespace scrap::Command
