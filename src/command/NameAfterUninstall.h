#pragma once

#include "command/ToolchainNaming.h"
#include "toolchain/InstalledToolchain.h"

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace scrap::Command {

/**
 * @brief A name that led to the uninstalled toolchain leads to no installed
 *        one now.
 */
struct LeadsNowhere { };

/**
 * @brief A name that led to the uninstalled toolchain leads to another
 *        installed version now.
 */
struct LeadsElsewhere {
    std::string name;  ///< <family>@<version>
};

/**
 * @brief Where a name that led to the uninstalled toolchain leads now.
 */
using NameAfterUninstall = std::variant<LeadsNowhere, LeadsElsewhere>;

/**
 * @brief Where @p named leads once @p removed is taken from @p installed.
 *
 * @param installed What was installed for the host, @p removed among it.
 * @return Where it leads now, or nothing when it did not lead to
 *         @p removed: it names the system's compiler or another toolchain.
 */
[[nodiscard]] std::optional<NameAfterUninstall> nameAfterUninstall(const NamedToolchain& named,
                                                                   const std::vector<Toolchain::InstalledName>& installed,
                                                                   std::string_view removed);

}  // namespace scrap::Command
