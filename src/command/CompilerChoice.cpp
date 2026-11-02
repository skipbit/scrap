#include "command/CompilerChoice.h"

#include "command/PrintableText.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "toolchain/GlobalConfigFile.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/SystemCompiler.h"
#include "toolchain/ToolchainChoice.h"
#include "toolchain/ToolchainSpecifier.h"

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace scrap::Command {

namespace {

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
 * @brief A toolchain named for the build, as written, and where.
 */
struct NamedToolchain {
    std::string text;
    ChoiceSource source = ChoiceSource::Variable;
};

std::string_view describeSource(const ChoiceSource source)
{
    switch (source) {
    case ChoiceSource::Variable:
        return "from SCRAP_TOOLCHAIN";
    case ChoiceSource::VariableOverProject:
        return "from SCRAP_TOOLCHAIN, overriding scrap.toml";
    case ChoiceSource::Project:
        return "from scrap.toml";
    case ChoiceSource::GlobalDefault:
        return "from the global default";
    }
    // Every source is answered above, so a source added without a word here
    // fails the build rather than being described as one of the others.
    std::unreachable();
}

std::string_view describeOrigin(const Toolchain::CompilerOrigin origin)
{
    switch (origin) {
    case Toolchain::CompilerOrigin::CompilerVariable:
        return "from CXX";
    case Toolchain::CompilerOrigin::DefaultOnPath:
        return "found on PATH";
    case Toolchain::CompilerOrigin::KnownName:
        return "found on PATH as a known name";
    }
    // Every origin is answered above, so an origin added without a word here
    // fails the build rather than being described as one of the others.
    std::unreachable();
}

/**
 * The toolchain named for the build, in order of precedence, or nothing
 * when none is named.
 */
std::expected<std::optional<NamedToolchain>, std::string> namedToolchain(const RuntimeEnvironment& env, const std::optional<std::string>& projectToolchain)
{
    if (! env.requestedToolchain.empty()) {
        return NamedToolchain{ .text = env.requestedToolchain,
                               .source = projectToolchain.has_value() ? ChoiceSource::VariableOverProject : ChoiceSource::Variable };
    }
    if (projectToolchain.has_value()) {
        return NamedToolchain{ .text = *projectToolchain, .source = ChoiceSource::Project };
    }
    if (! env.scrapHome.has_value()) {
        return std::nullopt;
    }
    const std::filesystem::path file = Toolchain::globalConfigFile(*env.scrapHome);
    const auto read = Toolchain::readGlobalDefaultFile(file);
    if (! read.has_value()) {
        return std::unexpected(renderGlobalConfigFileError(file, read.error()));
    }
    const std::optional<std::string>& globalDefault = *read;
    if (! globalDefault.has_value()) {
        return std::nullopt;
    }
    return NamedToolchain{ .text = *globalDefault, .source = ChoiceSource::GlobalDefault };
}

/**
 * The compiler the system provides. When nothing named a toolchain and
 * there is none, an installed toolchain is shown as one to select, not used.
 */
std::expected<ChosenCompiler, std::string> systemCompiler(const RuntimeEnvironment& env, const bool anythingNamed)
{
    auto found = Toolchain::detectSystemCompiler(env.preferredCompiler, env.systemSearchPaths);
    if (found.has_value()) {
        std::string announcement = "Using the system compiler '" + printablePath(found->path) + "' (";
        announcement += describeOrigin(found->origin);
        announcement += ")\n";
        return ChosenCompiler{ .compiler = std::move(found->path), .runtimeLibraryDirectories = {}, .announcement = std::move(announcement) };
    }
    if (! found.error().requested.empty()) {
        return std::unexpected(renderUnusableCompilerRequest(found.error().requested));
    }
    const auto host = Toolchain::hostEnvironment();
    if (! anythingNamed && env.scrapHome.has_value() && host.has_value()) {
        if (const auto example = Toolchain::anyInstalledToolchain(*env.scrapHome, *host); example.has_value()) {
            return std::unexpected(renderNoCompilerWithInstalled(example->name()));
        }
    }
    return std::unexpected(renderNoCompilerFound());
}

/**
 * The compiler of the newest installed toolchain @p specifier names.
 */
std::expected<ChosenCompiler, std::string> installedCompiler(const RuntimeEnvironment& env,
                                                             const NamedToolchain& named,
                                                             const Toolchain::ToolchainSpecifier& specifier)
{
    const auto scrapHome = requireScrapHome(env);
    if (! scrapHome.has_value()) {
        return std::unexpected(scrapHome.error());
    }
    auto found = Toolchain::findInstalledToolchainForHost(*scrapHome, specifier);
    if (! found.has_value()) {
        return std::unexpected(renderInstalledToolchainFault(named.text, found.error()));
    }
    std::string announcement = "Using the toolchain '" + printableText(found->name()) + "' (";
    announcement += describeSource(named.source);
    announcement += ")\n";
    std::filesystem::path compiler = found->compiler;
    return ChosenCompiler{ .compiler = std::move(compiler),
                           .runtimeLibraryDirectories = std::move(found->runtimeLibraryDirectories),
                           .announcement = std::move(announcement) };
}

}  // anonymous namespace

std::expected<ChosenCompiler, std::string> chooseCompiler(const RuntimeEnvironment& env, const std::optional<std::string>& projectToolchain)
{
    const auto named = namedToolchain(env, projectToolchain);
    if (! named.has_value()) {
        return std::unexpected(named.error());
    }
    const std::optional<NamedToolchain>& toolchain = *named;
    if (! toolchain.has_value()) {
        return systemCompiler(env, false);
    }
    const auto choice = Toolchain::parseToolchainChoice(toolchain->text);
    if (! choice.has_value()) {
        // Only SCRAP_TOOLCHAIN reaches here: scrap.toml and config.toml are
        // checked as they are read.
        return std::unexpected(renderToolchainVariableNotASpecifier(toolchain->text, choice.error()));
    }
    if (const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&*choice)) {
        return installedCompiler(env, *toolchain, *specifier);
    }
    return systemCompiler(env, true);
}

}  // namespace scrap::Command
