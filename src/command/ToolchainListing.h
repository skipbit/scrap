#pragma once

#include "command/ToolchainNaming.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/SystemCompiler.h"

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace scrap::Command {

/**
 * @brief Nothing names a toolchain here, so the system's compiler is used.
 */
struct NothingNamedHere { };

/**
 * @brief What names the toolchain used here cannot be read.
 */
struct NameHereUnreadable { };

/**
 * @brief What names the toolchain used in a project.
 */
using NamedHere = std::variant<NothingNamedHere, NameHereUnreadable, NamedToolchain>;

/**
 * @brief Why a toolchain is the one used here: where it was named or, when
 *        nothing names one, how the system's compiler was found.
 */
using UseReason = std::variant<ChoiceSource, Toolchain::CompilerOrigin>;

/**
 * @brief One toolchain to choose from, or one named that is not there.
 */
struct ListedToolchain {
    std::string name;                               ///< system, <family>@<version>, or a name as written that is not there.
    bool isSystem = false;                          ///< Whether it stands for the system's compiler.
    std::optional<std::filesystem::path> compiler;  ///< The system's compiler, when it is found.
    bool present = true;                            ///< Whether it is installed or, for system, found.
    bool isDefault = false;                         ///< Whether the global default names it.
    std::optional<UseReason> usedHere;              ///< Why it is used here, when it is.
};

/**
 * @brief The toolchains to choose from, as a listing shows them.
 */
struct ToolchainListing {
    std::vector<ListedToolchain> toolchains;
    bool inProject = false;  ///< Whether there is a project here to use one in.
};

/**
 * @brief What a listing is made from, as read by the caller.
 */
struct ListingFacts {
    std::vector<Toolchain::InstalledName> installed;          ///< By family, then from the oldest version.
    std::optional<Toolchain::SystemCompiler> systemCompiler;  ///< When the system has one.
    std::optional<NamedToolchain> globalDefault;              ///< When one is set and can be read.
    std::optional<NamedHere> here;                            ///< Nothing outside a project.
};

/**
 * @brief The toolchains to choose from, marked as the global default and
 *        as the one a build here uses.
 *
 * The system's compiler comes first when it is found, then what is
 * installed. A specifier marks the newest installed version it names, as
 * a build uses it. A name that is not there is added at the end, once, as
 * written, so it is seen where a build would stop.
 */
[[nodiscard]] ToolchainListing listToolchains(const ListingFacts& facts);

}  // namespace scrap::Command
