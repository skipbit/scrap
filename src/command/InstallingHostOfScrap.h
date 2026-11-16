#pragma once

#include "toolchain/HostEnvironment.h"
#include "toolchain/IngotSelection.h"

namespace scrap::Command {

/**
 * @brief What an ingot must suit to be installed by this scrap on
 *        @p environment.
 *
 * The version of this scrap is read from the banner: "scrap 0.1.0" or "scrap
 * 0.1.0 (v0.1.0-2-g2a761c0)". A banner that holds none gives the lowest
 * version, which then meets no requirement of the index.
 */
[[nodiscard]] Toolchain::InstallingHost installingHostOfScrap(const Toolchain::HostEnvironment& environment);

}  // namespace scrap::Command
