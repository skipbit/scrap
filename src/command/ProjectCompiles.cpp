#include "command/ProjectCompiles.h"

#include "command/CompilerChoice.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "compile/BuildProfile.h"
#include "compile/CompilationDatabase.h"
#include "compile/CompilePlanner.h"
#include "compile/CompilerDriver.h"
#include "project/Manifest.h"
#include "project/ProjectLoader.h"
#include "project/SourceCollector.h"
#include "project/TargetResolver.h"
#include "toolchain/CompilerIdentity.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/MacSdk.h"
#include "toolchain/ToolchainArguments.h"

#include <algorithm>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * The library among @p sources that has none, or nothing when every library
 * has sources.
 */
const Project::TargetSources* libraryWithoutSources(const std::vector<Project::TargetSources>& sources)
{
    const auto found = std::ranges::find_if(sources, [](const Project::TargetSources& each) {
        return (each.target.kind == Project::TargetKind::Library) && each.sources.empty();
    });
    return (found != sources.end()) ? &*found : nullptr;
}

/**
 * The sources of each of @p targets. What below src/ is left out is named.
 * Sources that cannot be decided, or a library left with nothing to build it
 * from, are reported.
 */
std::expected<std::vector<Project::TargetSources>, int> sourcesToBuild(const Project::LoadedProject& project,
                                                                       const std::vector<Project::Target>& targets)
{
    auto collected = Project::collectSources(project.root, targets);
    if (! collected.has_value()) {
        std::cerr << renderSourceCollectionError(collected.error());
        return std::unexpected(1);
    }
    for (const Project::LeftOutEntry& entry : collected->leftOut) {
        std::cerr << renderLeftOut(entry);
    }
    if (const Project::TargetSources* const found = libraryWithoutSources(collected->targets); found != nullptr) {
        std::cerr << renderLibraryWithoutSources(found->target.name);
        return std::unexpected(1);
    }
    return std::move(collected->targets);
}

/**
 * Whether any of @p targets has a C source.
 */
CSources cSourcesAmong(const std::vector<Project::TargetSources>& targets)
{
    const bool present = std::ranges::any_of(targets, [](const Project::TargetSources& entry) {
        return std::ranges::any_of(entry.sources, [](const std::filesystem::path& source) {
            return Project::languageOf(source) == Project::SourceLanguage::C;
        });
    });
    return present ? CSources::Present : CSources::Absent;
}

/**
 * The compiler the build of @p project uses, and its C compiler for a build
 * with @p cSources, named on standard error with where they came from. When
 * none can be used, the reason is reported instead.
 */
std::expected<ChosenCompiler, int> findCompiler(const RuntimeEnvironment& env, const Project::LoadedProject& project, const CSources cSources)
{
    auto chosen = chooseCompiler(env, project.manifest.toolchain, cSources);
    if (! chosen.has_value()) {
        std::cerr << chosen.error();
        return std::unexpected(1);
    }
    std::cerr << chosen->announcement;
    return std::move(*chosen);
}

/**
 * What @p compiler is given to compile and link against the macOS SDK.
 * Reports an SDK that cannot be found.
 */
std::expected<std::vector<std::string>, int> sdkArgumentsFor(const RuntimeEnvironment& env, const ChosenCompiler& compiler)
{
    const std::optional<Toolchain::HostEnvironment> host = Toolchain::hostEnvironment();
    const std::string_view platform = host.has_value() ? host->platform : std::string_view{};
    auto arguments = Toolchain::sdkArguments(! compiler.installed, platform, env.sdkRoot, Toolchain::askXcrunForSdkPath);
    if (! arguments.has_value()) {
        std::cerr << renderSdkNotFound(compiler.compiler);
        return std::unexpected(1);
    }
    return std::move(*arguments);
}

/**
 * @p arguments with @p sdk added.
 */
std::vector<std::string> withSdk(std::vector<std::string> arguments, const std::vector<std::string>& sdk)
{
    arguments.insert(arguments.end(), sdk.begin(), sdk.end());
    return arguments;
}

/**
 * What compiles the C sources of @p project with @p compiler: the C standard
 * it states, and the macOS SDK. The toolchain's compile arguments choose a
 * C++ library and are not given.
 */
Compile::CCompileSettings cSettingsFor(const Project::LoadedProject& project, const std::filesystem::path& compiler, const std::vector<std::string>& sdk)
{
    return Compile::CCompileSettings{ .compiler = compiler,
                                      .driver = Compile::CompilerDriver{ Toolchain::identifyCCompiler(compiler) },
                                      .standard = project.manifest.package.cStandard,
                                      .arguments = sdk };
}

/**
 * The settings to build @p project with: the compilers it uses, named on
 * standard error with where they came from, and what they are given. The C
 * compiler is looked for only for a build with @p cSources. The macOS SDK
 * is asked for before the compilers are identified.
 */
std::expected<Compile::BuildSettings, int> settingsFor(const RuntimeEnvironment& env,
                                                       const Project::LoadedProject& project,
                                                       const Compile::BuildProfile profile,
                                                       const std::filesystem::path& buildDirectory,
                                                       const CSources cSources)
{
    const auto compiler = findCompiler(env, project, cSources);
    if (! compiler.has_value()) {
        return std::unexpected(compiler.error());
    }
    const auto sdk = sdkArgumentsFor(env, *compiler);
    if (! sdk.has_value()) {
        return std::unexpected(sdk.error());
    }
    auto cSettings = compiler->cCompiler.transform([&project, &sdk](const std::filesystem::path& cCompiler) {
        return cSettingsFor(project, cCompiler, *sdk);
    });
    return Compile::BuildSettings{
        .projectRoot = project.root,
        .buildDirectory = buildDirectory,
        .cxx = Compile::CxxCompileSettings{ .compiler = compiler->compiler,
                                            .driver = Compile::CompilerDriver{ Toolchain::identifyCompiler(compiler->compiler) },
                                            .standard = project.manifest.package.standard,
                                            .arguments = withSdk(compiler->arguments.compile, *sdk) },
        .profile = profile,
        .runtimeLibraryDirectories = compiler->runtimeLibraryDirectories,
        .linkArguments = withSdk(compiler->arguments.link, *sdk),
        .c = std::move(cSettings)
    };
}

/**
 * Whether the compiler of @p language can build the standard it states. One
 * that cannot is reported.
 */
template <typename Standard>
bool supportsTheStandard(const Compile::LanguageSettings<Standard>& language)
{
    if (language.driver.standardOption(language.standard).has_value()) {
        return true;
    }
    std::cerr << renderUnsupportedStandard(language.compiler, language.standard);
    return false;
}

/**
 * Whether the compilers of @p settings can build the standards it states.
 * Each one that cannot is reported, the C++ one first, so both are known at
 * once.
 */
bool supportsTheStandards(const Compile::BuildSettings& settings)
{
    const bool cxxSupported = supportsTheStandard(settings.cxx);
    const bool cSupported = ! settings.c.has_value() || supportsTheStandard(*settings.c);
    return cxxSupported && cSupported;
}

}  // anonymous namespace

std::expected<std::vector<Project::Target>, int> resolveProjectTargets(const Project::LoadedProject& project)
{
    auto targets = Project::resolveTargets(project.root, project.manifest);
    if (! targets.has_value()) {
        std::cerr << renderNoTargetToBuild(project.root);
        return std::unexpected(1);
    }
    if (std::ranges::count(*targets, Project::TargetKind::Library, &Project::Target::kind) > 1) {
        std::cerr << renderSeveralLibraries();
        return std::unexpected(1);
    }
    return std::move(*targets);
}

std::expected<ProjectCompiles, int> planProjectCompiles(const RuntimeEnvironment& env,
                                                        const Project::LoadedProject& project,
                                                        const Compile::BuildProfile profile,
                                                        const std::vector<Project::Target>& targets)
{
    auto sources = sourcesToBuild(project, targets);
    if (! sources.has_value()) {
        return std::unexpected(sources.error());
    }
    const std::filesystem::path buildDirectory{ Compile::buildDirectoryOf(profile) };
    auto settings = settingsFor(env, project, profile, buildDirectory, cSourcesAmong(*sources));
    if (! settings.has_value()) {
        return std::unexpected(settings.error());
    }
    auto commands = Compile::planCompileCommands(*settings, *sources);
    const auto written = Compile::writeCompilationDatabase(project.root / buildDirectory, commands);
    if (! written.has_value()) {
        std::cerr << renderCompilationDatabaseFailure(written.error());
        return std::unexpected(1);
    }
    if (! supportsTheStandards(*settings)) {
        return std::unexpected(1);
    }
    return ProjectCompiles{ .settings = std::move(*settings), .sources = std::move(*sources), .commands = std::move(commands) };
}

}  // namespace scrap::Command
