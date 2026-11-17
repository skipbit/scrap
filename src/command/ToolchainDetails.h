#pragma once

#include "command/ToolchainListing.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/SystemCompiler.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace scrap::Command {

/**
 * @brief Whether a toolchain is the global default and the one a build
 *        here uses.
 */
struct ToolchainSelection {
    bool isDefault = false;
    std::optional<UseReason> usedHere;  ///< Why it is used here, when it is.
    bool inProject = false;             ///< Whether there is a project here to use one in.
};

/**
 * @brief A toolchain installed for the host, as its install record tells
 *        it.
 */
struct InstalledDetails {
    std::string name;  ///< <family>@<version>
    std::filesystem::path directory;
    Toolchain::InstallRecord record;
    ToolchainSelection selection;
};

/**
 * @brief A toolchain that is not installed, as the saved copy of the index
 *        tells it.
 */
struct IndexedDetails {
    Toolchain::IndexedLookup lookup;
    std::optional<std::string> fetchedAt;  ///< When the copy was fetched, in UTC. Nothing when unknown.
};

/**
 * @brief The system's compiler, found.
 */
struct SystemDetails {
    Toolchain::SystemCompiler compiler;
    ToolchainSelection selection;
};

/**
 * @brief What is told of one toolchain.
 */
using ToolchainDetails = std::variant<InstalledDetails, IndexedDetails, SystemDetails>;

/**
 * @brief How @p listing marks the toolchain named @p name: system, or
 *        <family>@<version>. A toolchain it does not list is not marked.
 */
[[nodiscard]] ToolchainSelection selectionIn(const ToolchainListing& listing, std::string_view name);

}  // namespace scrap::Command
