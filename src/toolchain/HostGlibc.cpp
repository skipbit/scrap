#include "toolchain/HostGlibc.h"

#include "toolchain/ToolchainVersion.h"

#include <optional>

#if __has_include(<gnu/libc-version.h>)
#include <gnu/libc-version.h>
#endif

namespace scrap::Toolchain {

std::optional<ToolchainVersion> hostGlibcVersion()
{
#if __has_include(<gnu/libc-version.h>)
    return ToolchainVersion::parse(gnu_get_libc_version());
#else
    return std::nullopt;
#endif
}

}  // namespace scrap::Toolchain
