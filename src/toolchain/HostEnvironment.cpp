#include "toolchain/HostEnvironment.h"

#include <optional>
#include <string>
#include <string_view>

namespace scrap::Toolchain {

namespace {

std::optional<std::string_view> hostPlatform()
{
#if defined(__linux__)
    return "linux";
#elif defined(__APPLE__)
    return "darwin";
#else
    return std::nullopt;
#endif
}

std::optional<std::string_view> hostArch()
{
#if defined(__x86_64__)
    return "x86_64";
#elif defined(__aarch64__)
    return "aarch64";
#else
    return std::nullopt;
#endif
}

}  // anonymous namespace

std::string HostEnvironment::directoryName() const
{
    std::string name{ platform };
    name += '-';
    name += arch;
    return name;
}

std::optional<HostEnvironment> hostEnvironment()
{
    const std::optional<std::string_view> platform = hostPlatform();
    const std::optional<std::string_view> arch = hostArch();
    if (! platform.has_value() || ! arch.has_value()) {
        return std::nullopt;
    }
    return HostEnvironment{ .platform = *platform, .arch = *arch };
}

}  // namespace scrap::Toolchain
