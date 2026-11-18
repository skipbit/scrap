#include "toolchain/StatusWarning.h"

#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <algorithm>
#include <optional>
#include <string>

namespace scrap::Toolchain {

namespace {

const IndexedToolchain* newestActive(const ToolchainIndex& index, const std::string& family, const InstallingHost& host)
{
    const IndexedToolchain* newest = nullptr;
    for (const IndexedToolchain& toolchain : index.toolchains) {
        if ((toolchain.family != family) || (toolchain.status != ToolchainStatus::Active) || (! canBeInstalled(toolchain, host))) {
            continue;
        }
        if ((newest == nullptr) || (toolchain.version > newest->version)) {
            newest = &toolchain;
        }
    }
    return newest;
}

}  // anonymous namespace

std::optional<StatusWarning> statusWarningFor(const ToolchainIndex& index, const std::string& family, const ToolchainVersion& version, const InstallingHost& host)
{
    const auto found = std::ranges::find_if(index.toolchains, [&family, &version](const IndexedToolchain& toolchain) {
        return (toolchain.family == family) && (toolchain.version == version);
    });
    if ((found == index.toolchains.end()) || (found->status == ToolchainStatus::Active) || (! isStatusRead(barrierTo(*found, index, host)))) {
        return std::nullopt;
    }
    const IndexedToolchain* alternative = newestActive(index, family, host);
    const bool offersAlternative = (alternative != nullptr) && ((found->status == ToolchainStatus::Disabled) || (alternative->version > version));
    return StatusWarning{ .name = found->text(),
                          .status = found->status,
                          .alternative = offersAlternative ? std::optional{ alternative->text() } : std::nullopt };
}

}  // namespace scrap::Toolchain
