#pragma once

#include "command/NameAfterUninstall.h"
#include "command/ToolchainNaming.h"
#include "install/ToolchainRemoval.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Command {

/**
 * @brief The line that says @p name was taken away from @p directory.
 */
[[nodiscard]] std::string renderUninstalled(std::string_view name, const std::filesystem::path& directory);

/**
 * @brief The line that says what became of the global default @p named,
 *        which led to the uninstalled toolchain: unselected, or leading to
 *        another version.
 */
[[nodiscard]] std::string renderGlobalDefaultAfterUninstall(const NamedToolchain& named, const NameAfterUninstall& after);

/**
 * @brief The lines that say what became of @p named, which SCRAP_TOOLCHAIN
 *        or the project names for builds here: a warning that they stop,
 *        with how to go on, or the version they use now.
 */
[[nodiscard]] std::string renderUsedHereAfterUninstall(const NamedToolchain& named, const NameAfterUninstall& after);

/**
 * @brief The error that the system's compiler is not scrap's to uninstall.
 */
[[nodiscard]] std::string renderCannotUninstallSystem();

/**
 * @brief The error and hint lines for @p specifier, which names nothing
 *        installed for the host.
 */
[[nodiscard]] std::string renderNothingToUninstall(std::string_view specifier);

/**
 * @brief The error and hint lines for @p specifier, which names each of
 *        @p names, from the oldest.
 */
[[nodiscard]] std::string renderNamesMoreThanOneToUninstall(std::string_view specifier, const std::vector<std::string>& names);

/**
 * @brief The warning that what could not be deleted is left at @p path.
 */
[[nodiscard]] std::string renderLeftBehind(const std::filesystem::path& path);

/**
 * @brief The error line for a toolchain that could not be taken away.
 */
[[nodiscard]] std::string renderNotRemoved(const Install::ToolchainNotRemoved& failure);

}  // namespace scrap::Command
