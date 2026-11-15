#pragma once

#include "command/RuntimeEnvironment.h"
#include "toolchain/ToolchainVersion.h"

#include <string>

namespace scrap::Command {

/**
 * @brief The URL of the toolchain index: SCRAP_TOOLCHAIN_INDEX when it is
 *        set, the official channel's index otherwise.
 */
[[nodiscard]] std::string toolchainIndexUrl(const RuntimeEnvironment& env);

/**
 * @brief The version of this scrap, which the index is read for.
 *
 * It is read from the banner: "scrap 0.1.0" or "scrap 0.1.0
 * (v0.1.0-2-g2a761c0)". A banner that holds none gives the lowest version,
 * which then meets no requirement of the index.
 */
[[nodiscard]] Toolchain::ToolchainVersion runningScrapVersion();

}  // namespace scrap::Command
