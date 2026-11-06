#pragma once

#include "toolchain/ToolchainVersion.h"

#include <optional>

namespace scrap::Toolchain {

/**
 * @brief The version of the glibc this process runs on, or nothing on a host
 *        without one.
 */
[[nodiscard]] std::optional<ToolchainVersion> hostGlibcVersion();

}  // namespace scrap::Toolchain
