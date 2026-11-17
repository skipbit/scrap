#pragma once

#include "toolchain/IngotSelection.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ToolchainIndex.h"

#include <optional>
#include <string>
#include <vector>

namespace scrap::Command {

/**
 * @brief A toolchain of the index, as a listing of the index shows it.
 */
struct RemoteToolchain {
    std::string name;  ///< <family>@<version>
    Toolchain::ToolchainStatus status = Toolchain::ToolchainStatus::Active;
    bool installed = false;                                 ///< Whether it is installed for the host.
    std::optional<Toolchain::IngotSelectionFault> barrier;  ///< What stops the host installing it; nothing when it can.
};

/**
 * @brief The toolchains of @p index, by family and then from the newest
 *        version, each marked as installed when @p installed holds it.
 *
 * @param all Whether to list those the host cannot install too, rather
 *            than only those it can.
 */
[[nodiscard]] std::vector<RemoteToolchain> listRemoteToolchains(const Toolchain::ToolchainIndex& index,
                                                                const std::vector<Toolchain::InstalledName>& installed,
                                                                const Toolchain::InstallingHost& host,
                                                                bool all);

}  // namespace scrap::Command
