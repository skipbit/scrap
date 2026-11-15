#include "command/ToolchainIndexLocation.h"

#include "command/RuntimeEnvironment.h"
#include "shared/constants/version.h"
#include "toolchain/ToolchainVersion.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace scrap::Command {

namespace {

constexpr std::string_view OfficialIndexUrl = "https://raw.githubusercontent.com/skipbit/scrap-toolchain/main/index.toml";

}  // anonymous namespace

std::string toolchainIndexUrl(const RuntimeEnvironment& env)
{
    return env.toolchainIndexUrl.empty() ? std::string{ OfficialIndexUrl } : env.toolchainIndexUrl;
}

Toolchain::ToolchainVersion runningScrapVersion()
{
    const std::string banner = version();
    const std::size_t start = banner.find(' ') + 1;
    const std::string text = banner.substr(start, banner.find(' ', start) - start);
    return Toolchain::ToolchainVersion::parse(text).value_or(Toolchain::ToolchainVersion::lowest());
}

}  // namespace scrap::Command
