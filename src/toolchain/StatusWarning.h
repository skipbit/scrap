#pragma once

#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <optional>
#include <string>

namespace scrap::Toolchain {

/**
 * @brief The index no longer offers a toolchain as it did: it is deprecated,
 *        or no longer provided.
 */
struct StatusWarning {
    std::string name;                        ///< <family>@<version>
    ToolchainStatus status;                  ///< Deprecated or Disabled.
    std::optional<std::string> alternative;  ///< Another toolchain to install instead, as <family>@<version>, when there is one.
};

/**
 * @brief What to warn of @p family at @p version, as @p index has it for
 *        @p host, or nothing when it is active or its status is not known.
 *
 * The alternative is the newest active version of the family that can be
 * installed on @p host; for a deprecated version, only when it is newer.
 */
[[nodiscard]] std::optional<StatusWarning> statusWarningFor(const ToolchainIndex& index,
                                                            const std::string& family,
                                                            const ToolchainVersion& version,
                                                            const InstallingHost& host);

}  // namespace scrap::Toolchain
