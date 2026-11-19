#include "toolchain/StatusWarning.h"

#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <algorithm>
#include <optional>
#include <string>

namespace scrap::Toolchain {

namespace {

/**
 * Whether a toolchain of @p status at @p version is told of @p alternative: a
 * disabled one of any, a deprecated one of a newer one only.
 */
bool offers(const ToolchainStatus status, const IndexedToolchain& alternative, const ToolchainVersion& version)
{
    switch (status) {
    case ToolchainStatus::Active:
        return false;
    case ToolchainStatus::Deprecated:
        return alternative.version > version;
    case ToolchainStatus::Disabled:
        return true;
    }
    return false;
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
    const IndexedToolchain* alternative = newestInstallable(index, family, host, SuggestedStatus::ActiveOnly);
    const bool offersAlternative = (alternative != nullptr) && offers(found->status, *alternative, version);
    return StatusWarning{ .name = found->text(),
                          .status = found->status,
                          .alternative = offersAlternative ? std::optional{ alternative->text() } : std::nullopt };
}

}  // namespace scrap::Toolchain
