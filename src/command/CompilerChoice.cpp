#include "command/CompilerChoice.h"

#include "command/PrintableText.h"
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
 * @brief A toolchain named for the build, as written and as understood, and
 *        where.
 */
struct NamedToolchain {
    std::string text;
    Toolchain::ToolchainChoice choice;
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

/**
 * Where a toolchain named by @p source was named, for a report.
 */
ToolchainNamedIn namedIn(const ChoiceSource source)
{
    switch (source) {
    case ChoiceSource::Variable:
    case ChoiceSource::VariableOverProject:
        return ToolchainNamedIn::Variable;
    case ChoiceSource::Project:
        return ToolchainNamedIn::Project;
    case ChoiceSource::GlobalDefault:
        return ToolchainNamedIn::GlobalDefault;
    }
    // Every source is answered above, so a source added without a place
    // here fails the build rather than being reported as another.
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
 * @p text, named by @p source, understood as a choice. A name that is no
 * choice is reported as the place that gave it.
 */
std::expected<NamedToolchain, std::string> parseNamed(std::string text, const ChoiceSource source)
{
    auto choice = Toolchain::parseToolchainChoice(text);
    if (! choice.has_value()) {
        switch (source) {
        case ChoiceSource::Variable:
        case ChoiceSource::VariableOverProject:
            return std::unexpected(renderToolchainVariableNotASpecifier(text, choice.error()));
        case ChoiceSource::Project:
        case ChoiceSource::GlobalDefault:
            return std::unexpected(renderNotASpecifier(text, choice.error()));
        }
        std::unreachable();
    }
    return NamedToolchain{ .text = std::move(text), .choice = std::move(*choice), .source = source };
}

/**
 * The toolchain named for the build, in order of precedence, or nothing
 * when none is named.
 */
std::expected<std::optional<NamedToolchain>, std::string> namedToolchain(const RuntimeEnvironment& env, const std::optional<std::string>& projectToolchain)
{
    if (! env.requestedToolchain.empty()) {
        return parseNamed(env.requestedToolchain, projectToolchain.has_value() ? ChoiceSource::VariableOverProject : ChoiceSource::Variable);
    }
    if (projectToolchain.has_value()) {
        return parseNamed(*projectToolchain, ChoiceSource::Project);
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
    return parseNamed(*globalDefault, ChoiceSource::GlobalDefault);
}

/**
 * The compiler the system provides, or why there is none.
 */
std::expected<ChosenCompiler, Toolchain::NoCompiler> systemCompiler(const RuntimeEnvironment& env)
{
    auto found = Toolchain::detectSystemCompiler(env.preferredCompiler, env.systemSearchPaths);
    if (found.has_value()) {
        std::string announcement = "Using the system compiler '" + printablePath(found->path) + "' (";
        announcement += describeOrigin(found->origin);
        announcement += ")\n";
        return ChosenCompiler{ .compiler = std::move(found->path), .runtimeLibraryDirectories = {}, .announcement = std::move(announcement) };
    }
    return std::unexpected(std::move(found.error()));
}

/**
 * The report for a system with no compiler.
 */
std::string renderNoSystemCompiler(const Toolchain::NoCompiler& missing)
{
    if (! missing.requested.empty()) {
        return renderUnusableCompilerRequest(missing.requested);
    }
    return renderNoCompilerFound();
}

/**
 * The report for a system with no compiler where nothing named a toolchain:
 * an installed toolchain is shown as one to select, not used.
 */
std::string renderNoCompilerWhereNothingIsNamed(const RuntimeEnvironment& env, const Toolchain::NoCompiler& missing)
{
    const auto host = Toolchain::hostEnvironment();
    if (missing.requested.empty() && env.scrapHome.has_value() && host.has_value()) {
        if (const auto example = Toolchain::anyInstalledToolchain(*env.scrapHome, *host); example.has_value()) {
            return renderNoCompilerWithInstalled(example->name());
        }
    }
    return renderNoSystemCompiler(missing);
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
        return std::unexpected(renderInstalledToolchainFault(named.text, namedIn(named.source), found.error()));
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
        auto system = systemCompiler(env);
        if (! system.has_value()) {
            return std::unexpected(renderNoCompilerWhereNothingIsNamed(env, system.error()));
        }
        return std::move(*system);
    }
    if (const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&toolchain->choice)) {
        return installedCompiler(env, *toolchain, *specifier);
    }
    auto system = systemCompiler(env);
    if (! system.has_value()) {
        return std::unexpected(renderNoSystemCompiler(system.error()));
    }
    return std::move(*system);
}

}  // namespace scrap::Command
