#include "command/RemoteListing.h"

#include "toolchain/IngotSelection.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ToolchainIndex.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace scrap::Command {

std::vector<RemoteToolchain> listRemoteToolchains(const Toolchain::ToolchainIndex& index,
                                                  const std::vector<Toolchain::InstalledName>& installed,
                                                  const Toolchain::InstallingHost& host)
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
        const bool isInstalled = std::ranges::any_of(installed, [toolchain](const Toolchain::InstalledName& each) {
            return (each.family == toolchain->family) && (each.version == toolchain->version);
        });
        rows.push_back(RemoteToolchain{ .name = toolchain->text(),
                                        .status = Toolchain::isStatusRead(barrier) ? std::optional{ toolchain->status } : std::nullopt,
                                        .installed = isInstalled,
                                        .barrier = std::move(barrier) });
    }
    return rows;
}

std::vector<RemoteToolchain> installableOnly(const std::vector<RemoteToolchain>& rows)
{
    std::vector<RemoteToolchain> installable;
    std::ranges::copy_if(rows, std::back_inserter(installable), [](const RemoteToolchain& row) {
        return ! row.barrier.has_value();
    });
    return installable;
}

}  // namespace scrap::Command
