#include "command/CompilerChoice.h"

#include "command/PrintableText.h"
#include "command/RuntimeEnvironment.h"
#include "command/SavedStatusWarning.h"
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
                               .announcement = std::move(announcement),
                               .cCompiler = std::nullopt };
    }
    return std::unexpected(std::move(found.error()));
}

/**
 * The name of a toolchain installed for this host, to show as one to select:
 * one that can build C, for a build with @p cSources.
 */
std::optional<std::string> installedExample(const RuntimeEnvironment& env, const CSources cSources)
{
    const auto host = Toolchain::hostEnvironment();
    if (! env.scrapHome.has_value() || ! host.has_value()) {
        return std::nullopt;
    }
    const auto buildsWhatIsAsked = [cSources](const Toolchain::InstalledToolchain& toolchain) {
        return cSources == CSources::Absent || Toolchain::usableCCompiler(toolchain).has_value();
    };
    return Toolchain::anyInstalledToolchain(*env.scrapHome, *host, buildsWhatIsAsked).transform(&Toolchain::InstalledToolchain::name);
}

/**
 * The report for a system with no C compiler. Where nothing named a
 * toolchain, so @p source is empty, an installed toolchain is shown as one
 * to select, not used.
 */
std::string renderNoSystemCCompiler(const RuntimeEnvironment& env, const std::optional<ChoiceSource> source, const Toolchain::NoCompiler& missing)
{
    if (! missing.requested.empty()) {
        return renderUnusableCCompilerRequest(missing.requested);
    }
    if (! source.has_value()) {
        if (const auto example = installedExample(env, CSources::Present); example.has_value()) {
            return renderNoCCompilerWithInstalled(*example);
        }
    }
    return renderNoCCompilerFound();
}

/**
 * @p chosen with the C compiler the system provides, named on a line of its
 * own, or the report of why there is none.
 */
std::expected<ChosenCompiler, std::string> withSystemCCompiler(const RuntimeEnvironment& env, const std::optional<ChoiceSource> source, ChosenCompiler chosen)
{
    auto found = Toolchain::detectSystemCCompiler(env.preferredCCompiler, env.systemSearchPaths);
    if (! found.has_value()) {
        return std::unexpected(renderNoSystemCCompiler(env, source, found.error()));
    }
    chosen.announcement += "Using the system C compiler '" + printablePath(found->path) + "' (";
    chosen.announcement += describeCOrigin(found->origin);
    chosen.announcement += ")\n";
    chosen.cCompiler = std::move(found->path);
    return chosen;
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
    if (missing.requested.empty()) {
        if (const auto example = installedExample(env, CSources::Absent); example.has_value()) {
            return renderNoCompilerWithInstalled(*example);
        }
    }
    return renderNoSystemCompiler(missing);
}

/**
 * The compiler the system provides for a build with @p cSources, or the
 * report of why there is none. The line naming it says where system was
 * named, when @p source says it was.
 */
std::expected<ChosenCompiler, std::string> systemCompilers(const RuntimeEnvironment& env,
                                                           const std::optional<ChoiceSource> source,
                                                           const CSources cSources)
{
    auto system = systemCompiler(env, source);
    if (! system.has_value()) {
        return std::unexpected(source.has_value() ? renderNoSystemCompiler(system.error()) : renderNoCompilerWhereNothingIsNamed(env, system.error()));
    }
    if (cSources == CSources::Absent) {
        return std::move(*system);
    }
    return withSystemCCompiler(env, source, std::move(*system));
}

/**
 * The compiler of the newest installed toolchain @p specifier names, with
 * the C compiler it records for a build with @p cSources.
 */
std::expected<ChosenCompiler, std::string> installedCompiler(const RuntimeEnvironment& env,
                                                             const NamedToolchain& named,
                                                             const Toolchain::ToolchainSpecifier& specifier,
                                                             const CSources cSources)
{
    const auto scrapHome = requireScrapHome(env);
    if (! scrapHome.has_value()) {
        return std::unexpected(scrapHome.error());
    }
    auto found = Toolchain::findInstalledToolchainForHost(*scrapHome, specifier);
    if (! found.has_value()) {
        return std::unexpected(renderInstalledToolchainFault(named.text, namedIn(named.source), found.error()));
    }
    std::optional<std::filesystem::path> cCompiler;
    if (cSources == CSources::Present) {
        auto usable = Toolchain::usableCCompiler(*found);
        if (! usable.has_value()) {
            const Toolchain::UnusableCCompiler& unusable = usable.error();
            return std::unexpected(unusable.named ? renderUnusableInstalled(found->name(), unusable.reason) : renderNoCCompilerRecorded(found->name(), unusable.reason));
        }
        cCompiler = std::move(*usable);
    }
    std::string announcement = "Using the toolchain '" + printableText(found->name()) + "' (";
    announcement += describeSource(named.source);
    announcement += ")\n";
    announcement += SavedStatusWarning::read(env, *scrapHome).linesFor(found->family, found->version);
    std::filesystem::path compiler = found->compiler;
    return ChosenCompiler{ .compiler = std::move(compiler),
                           .runtimeLibraryDirectories = std::move(found->runtimeLibraryDirectories),
                           .arguments = std::move(found->arguments),
                           .installed = true,
                           .announcement = std::move(announcement),
                           .cCompiler = std::move(cCompiler) };
}

}  // anonymous namespace

std::expected<ChosenCompiler, std::string> chooseCompiler(const RuntimeEnvironment& env,
                                                          const std::optional<std::string>& projectToolchain,
                                                          const CSources cSources)
{
    const auto named = namedToolchain(env, projectToolchain);
    if (! named.has_value()) {
        return std::unexpected(named.error());
    }
    const std::optional<NamedToolchain>& toolchain = *named;
    if (! toolchain.has_value()) {
        return systemCompilers(env, std::nullopt, cSources);
    }
    if (const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&toolchain->choice)) {
        return installedCompiler(env, *toolchain, *specifier, cSources);
    }
    return systemCompilers(env, toolchain->source, cSources);
}

}  // namespace scrap::Command
