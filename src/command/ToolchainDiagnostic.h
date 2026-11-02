#pragma once

#include "toolchain/GlobalConfigFile.h"
#include "toolchain/InstalledToolchain.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief Report @p text, given on the command line, as no specifier, for
 *        @p reason.
 */
[[nodiscard]] std::string renderNotASpecifier(std::string_view text, std::string_view reason);

/**
 * @brief Report SCRAP_TOOLCHAIN, holding @p value, as no specifier, for
 *        @p reason.
 */
[[nodiscard]] std::string renderToolchainVariableNotASpecifier(std::string_view value, std::string_view reason);

/**
 * @brief Report that no toolchain @p specifier names is installed, with the
 *        command that installs it.
 */
[[nodiscard]] std::string renderNotInstalled(std::string_view specifier);

/**
 * @brief Report that the installed toolchain @p specifier names cannot be
 *        used, for @p reason, with the command that installs it again.
 */
[[nodiscard]] std::string renderUnusableInstalled(std::string_view specifier, std::string_view reason);

/**
 * @brief Report that no toolchain is built for the host scrap runs on.
 */
[[nodiscard]] std::string renderNoToolchainForHost();

/**
 * @brief Report that there is no directory to keep toolchains in.
 */
[[nodiscard]] std::string renderNoScrapHome();

/**
 * @brief Report why the global default toolchain in @p file cannot be read
 *        or written.
 */
[[nodiscard]] std::string renderGlobalConfigFileError(const std::filesystem::path& file, const Toolchain::GlobalConfigFileError& error);

/**
 * @brief Report why no installed toolchain can be used for @p specifier,
 *        as the user wrote it.
 */
[[nodiscard]] std::string renderInstalledToolchainFault(std::string_view specifier, const Toolchain::InstalledToolchainFault& fault);

/**
 * @brief Report a system with no C++ compiler, and what to do next.
 */
[[nodiscard]] std::string renderNoCompilerFound();

/**
 * @brief Report a compiler the environment asked for and cannot be run.
 *
 * @param requested The value CXX gave.
 */
[[nodiscard]] std::string renderUnusableCompilerRequest(std::string_view requested);

/**
 * @brief Report a system without a C++ compiler where nothing chose a
 *        toolchain, with how to select one of those installed, such as
 *        @p example.
 */
[[nodiscard]] std::string renderNoCompilerWithInstalled(std::string_view example);

}  // namespace scrap::Command
