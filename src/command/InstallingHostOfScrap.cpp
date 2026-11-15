#include "command/InstallingHostOfScrap.h"

#include "shared/constants/version.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/HostGlibc.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainVersion.h"

#include <cstddef>
#include <string>

namespace scrap::Command {

namespace {

Toolchain::ToolchainVersion runningScrapVersion()
{
    const std::string banner = version();
    const std::size_t start = banner.find(' ') + 1;
    const std::string text = banner.substr(start, banner.find(' ', start) - start);
    return Toolchain::ToolchainVersion::parse(text).value_or(Toolchain::ToolchainVersion::lowest());
}

}  // anonymous namespace

Toolchain::InstallingHost installingHostOfScrap(const Toolchain::HostEnvironment& environment)
{
    return Toolchain::InstallingHost{ .environment = environment, .scrapVersion = runningScrapVersion(), .glibcVersion = Toolchain::hostGlibcVersion() };
}

}  // namespace scrap::Command
