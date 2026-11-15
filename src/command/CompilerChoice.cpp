#include "command/CompilerChoice.h"

#include "command/PrintableText.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainNaming.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/SystemCompiler.h"
#include "toolchain/ToolchainSpecifier.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace scrap::Command {

namespace {

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

/**
 * The compiler the system provides, or why there is none. The line naming
 * it says where system was named, when @p source says it was.
 */
std::expected<ChosenCompiler, Toolchain::NoCompiler> systemCompiler(const RuntimeEnvironment& env, const std::optional<ChoiceSource> source)
{
    auto found = Toolchain::detectSystemCompiler(env.preferredCompiler, env.systemSearchPaths);
    if (found.has_value()) {
        std::string announcement = "Using the system compiler '" + printablePath(found->path) + "' (";
        announcement += describeOrigin(found->origin);
        if (source.has_value()) {
            announcement += ", as system ";
            announcement += describeSource(*source);
        }
        announcement += ")\n";
        return ChosenCompiler{ .compiler = std::move(found->path),
                               .runtimeLibraryDirectories = {},
                               .arguments = { .compile = {}, .link = {} },
                               .installed = false,
                               .announcement = std::move(announcement) };
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
                           .arguments = std::move(found->arguments),
                           .installed = true,
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
        auto system = systemCompiler(env, std::nullopt);
        if (! system.has_value()) {
            return std::unexpected(renderNoCompilerWhereNothingIsNamed(env, system.error()));
        }
        return std::move(*system);
    }
    if (const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&toolchain->choice)) {
        return installedCompiler(env, *toolchain, *specifier);
    }
    auto system = systemCompiler(env, toolchain->source);
    if (! system.has_value()) {
        return std::unexpected(renderNoSystemCompiler(system.error()));
    }
    return std::move(*system);
}

}  // namespace scrap::Command
