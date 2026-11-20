#pragma once

#include "command/RuntimeEnvironment.h"
#include "toolchain/SystemCompiler.h"
#include "toolchain/ToolchainChoice.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief Where the toolchain a build uses was named.
 */
enum class ChoiceSource : std::uint8_t {
    Variable,             ///< SCRAP_TOOLCHAIN, with no [toolchain] name in the project.
    VariableOverProject,  ///< SCRAP_TOOLCHAIN, in place of the project's [toolchain] name.
    Project,              ///< The project's [toolchain] name.
    GlobalDefault         ///< The global default.
};

/**
 * @brief A toolchain named for the build, as written and as understood, and
 *        where.
 */
struct NamedToolchain {
    std::string text;
    Toolchain::ToolchainChoice choice;
    ChoiceSource source = ChoiceSource::Variable;
};

/**
 * @brief Where @p source named a toolchain, as the line naming it says it,
 *        such as "from scrap.toml".
 */
[[nodiscard]] std::string_view describeSource(ChoiceSource source);

/**
 * @brief How the system's compiler was found (@p origin), as the line
 *        naming it says it, such as "found on PATH".
 */
[[nodiscard]] std::string_view describeOrigin(Toolchain::CompilerOrigin origin);

/**
 * @brief How the system's C compiler was found (@p origin), as
 *        describeOrigin() says it of the C++ one, with CC for CXX.
 */
[[nodiscard]] std::string_view describeCOrigin(Toolchain::CompilerOrigin origin);

/**
 * @brief The toolchain SCRAP_TOOLCHAIN or the project's [toolchain] name
 *        (@p projectToolchain) names, in that order, before the global
 *        default is read.
 *
 * @return The toolchain, nothing when neither names one, or the error and
 *         hint lines for a name that is no choice.
 */
[[nodiscard]] std::expected<std::optional<NamedToolchain>, std::string> namedAboveGlobalDefault(const RuntimeEnvironment& env,
                                                                                                const std::optional<std::string>& projectToolchain);

/**
 * @brief What names a toolchain above the global default for the working
 *        directory, and whether a project is there.
 */
struct NamedAboveHere {
    std::optional<NamedToolchain> named;  ///< Nothing when nothing above the global default names one.
    bool inProject = false;
};

/**
 * @brief The toolchain SCRAP_TOOLCHAIN or the project in the working
 *        directory names, before the global default is read.
 *
 * Outside a project, a SCRAP_TOOLCHAIN that is no choice names nothing.
 *
 * @return What names it, or the error lines for a project or a name that
 *         cannot be read.
 */
[[nodiscard]] std::expected<NamedAboveHere, std::string> namedAboveGlobalDefaultHere(const RuntimeEnvironment& env);

/**
 * @brief The toolchain the global default names.
 *
 * @return The toolchain, nothing when SCRAP_HOME is not known or no
 *         default is set, or the error and hint lines when config.toml
 *         cannot be read or its default is no choice.
 */
[[nodiscard]] std::expected<std::optional<NamedToolchain>, std::string> namedGlobalDefault(const RuntimeEnvironment& env);

/**
 * @brief The toolchain named for a build, in order of precedence:
 *        SCRAP_TOOLCHAIN, the project's [toolchain] name, then the global
 *        default.
 *
 * The global default is read only when nothing before it names a
 * toolchain, so a config.toml that cannot be read stops only a build that
 * would use it.
 *
 * @return The toolchain, nothing when none is named, or the error and hint
 *         lines to report.
 */
[[nodiscard]] std::expected<std::optional<NamedToolchain>, std::string> namedToolchain(const RuntimeEnvironment& env, const std::optional<std::string>& projectToolchain);

}  // namespace scrap::Command
