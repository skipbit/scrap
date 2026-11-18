#pragma once

#include "toolchain/IngotSelection.h"
#include "toolchain/StatusWarning.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <string>

namespace scrap::Command {

/**
 * @brief The hint to install @p name, another version of what is no longer
 *        provided.
 */
[[nodiscard]] std::string anotherVersionHint(const std::string& name);

/**
 * @brief The warning that a toolchain is deprecated or no longer provided,
 *        with what to install instead when there is something.
 */
[[nodiscard]] std::string renderStatusWarning(const Toolchain::StatusWarning& warning);

/**
 * @brief The warning of @p family at @p version, as @p index has it for
 *        @p host; empty when there is nothing to warn of.
 */
[[nodiscard]] std::string renderStatusWarningOf(const Toolchain::ToolchainIndex& index,
                                                const std::string& family,
                                                const Toolchain::ToolchainVersion& version,
                                                const Toolchain::InstallingHost& host);

}  // namespace scrap::Command
