#include "command/SavedStatusWarning.h"

#include "command/InstallingHostOfScrap.h"
#include "command/RuntimeEnvironment.h"
#include "command/StatusWarningText.h"
#include "command/ToolchainIndexLocation.h"
#include "install/IndexCopy.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainVersion.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace scrap::Command {

SavedStatusWarning::SavedStatusWarning(std::optional<Copy> copy)
    : _copy(std::move(copy))
{
}

SavedStatusWarning SavedStatusWarning::read(const RuntimeEnvironment& env, const std::filesystem::path& scrapHome)
{
    const auto environment = Toolchain::hostEnvironment();
    if (! environment.has_value()) {
        return SavedStatusWarning{ std::nullopt };
    }
    Toolchain::InstallingHost host = installingHostOfScrap(*environment);
    auto saved = Install::readSavedIndex(scrapHome, toolchainIndexUrl(env), host.scrapVersion);
    if (! saved.has_value()) {
        return SavedStatusWarning{ std::nullopt };
    }
    std::optional<Install::SavedIndex>& copy = *saved;
    if (! copy.has_value()) {
        return SavedStatusWarning{ std::nullopt };
    }
    return SavedStatusWarning{ Copy{ .index = std::move(copy->index), .host = std::move(host) } };
}

std::string SavedStatusWarning::linesFor(const std::string& family, const Toolchain::ToolchainVersion& version) const
{
    if (! _copy.has_value()) {
        return {};
    }
    return renderStatusWarningOf(_copy->index, family, version, _copy->host);
}

}  // namespace scrap::Command
