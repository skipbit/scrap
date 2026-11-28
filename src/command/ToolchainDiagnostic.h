#pragma once

#include "toolchain/GlobalConfigFile.h"
#include "toolchain/InstalledToolchain.h"

#include <cstdint>
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
 * @brief Report @p text, given to install, as no specifier, for @p reason.
 */
[[nodiscard]] std::string renderNotInstallable(std::string_view text, std::string_view reason);

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
 *        used, for the reason @p fault gives, with the command that installs
 *        it again, or a hint to check permissions when looking at a path it
 *        needs was refused.
 */
[[nodiscard]] std::string renderUnusableInstalled(std::string_view specifier, const Toolchain::UnusableInstalled& fault);

/**
 * @brief Report that the installed toolchain @p specifier names records no C
 *        compiler, for @p reason, with the two ways to build C.
 *
 * An earlier scrap recorded none, and installing again records one. An index
 * may name none, and the compilers of the system can build C instead.
 */
[[nodiscard]] std::string renderNoCCompilerRecorded(std::string_view specifier, std::string_view reason);

/**
 * @brief Where a toolchain was named, so a report says where to name
 *        another.
 */
enum class ToolchainNamedIn : std::uint8_t {
    CommandLine,   ///< The argument of a command.
    Variable,      ///< SCRAP_TOOLCHAIN.
    Project,       ///< The project's [toolchain] name.
    GlobalDefault  ///< The global default.
};

/**
 * @brief Report that no toolchain is built for the host scrap runs on, with
 *        how to choose the system's compiler where the toolchain was named
 *        (@p namedIn).
 */
[[nodiscard]] std::string renderNoToolchainForHost(ToolchainNamedIn namedIn);

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
 *        as the user wrote it where @p namedIn says.
 */
[[nodiscard]] std::string renderInstalledToolchainFault(std::string_view specifier, ToolchainNamedIn namedIn, const Toolchain::InstalledToolchainFault& fault);

/**
 * @brief Report a directory of installed toolchains that cannot be listed.
 */
[[nodiscard]] std::string renderCannotListToolchains(const Toolchain::CannotListToolchains& fault);

/**
 * @brief Report that the global default @p name is unselected, so no
 *        toolchain is selected globally.
 */
[[nodiscard]] std::string renderUnselected(std::string_view name);

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
 * @brief Report a system with no C compiler for a build with C sources, and
 *        what to do next.
 */
[[nodiscard]] std::string renderNoCCompilerFound();

/**
 * @brief Report a C compiler the environment asked for and cannot be run.
 *
 * @param requested The value CC gave.
 */
[[nodiscard]] std::string renderUnusableCCompilerRequest(std::string_view requested);

/**
 * @brief Report a system without a C++ compiler where nothing chose a
 *        toolchain, with how to select one of those installed, such as
 *        @p example.
 */
[[nodiscard]] std::string renderNoCompilerWithInstalled(std::string_view example);

/**
 * @brief Report a system without a C compiler for a build with C sources
 *        where nothing chose a toolchain, with how to select one of those
 *        installed, such as @p example.
 */
[[nodiscard]] std::string renderNoCCompilerWithInstalled(std::string_view example);

/**
 * @brief The error lines @p rendered as a warning, for what a command goes
 *        on without.
 */
[[nodiscard]] std::string asWarning(std::string rendered);

}  // namespace scrap::Command
