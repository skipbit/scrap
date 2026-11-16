#include "command/ToolchainIndexLocation.h"

#include "command/RuntimeEnvironment.h"

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

}  // namespace scrap::Command
