#pragma once

#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainIndex.h"

#include <cstddef>
#include <string>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief A toolchain whose status changed between two indexes.
 */
struct ChangedToolchain {
    std::string name;  ///< e.g. gcc@15.2.0
    ToolchainStatus before;
    ToolchainStatus after;
};

/**
 * @brief What changed for a host between two indexes, each sorted by
 *        family, then by version.
 */
struct IndexChanges {
    std::vector<std::string> added;         ///< Toolchains that can be installed now, and could not be before.
    std::vector<ChangedToolchain> changed;  ///< Toolchains with an ingot for the host in both, whose status changed.
};

/**
 * @brief What changed for @p host from @p before to @p after.
 *
 * A toolchain whose status changed is told as changed, even when that is
 * what lets it be installed now. One that is gone, or has lost its ingot
 * for the host, is not told.
 */
[[nodiscard]] IndexChanges compareIndexes(const ToolchainIndex& before, const ToolchainIndex& after, const InstallingHost& host);

/**
 * @brief How many toolchains of @p index can be installed on @p host.
 */
[[nodiscard]] std::size_t countInstallable(const ToolchainIndex& index, const InstallingHost& host);

}  // namespace scrap::Toolchain
