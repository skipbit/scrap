#include "command/ProjectBuild.h"

#include "build/BuildSteps.h"
#include "build/Parallelism.h"
#include "build/StepHistory.h"
#include "build/StepRunner.h"
#include "command/BuildProgress.h"
#include "command/CompilerChoice.h"
#include "command/ParsedOptions.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "compile/ArchiveCommand.h"
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
#include "toolchain/SystemArchiver.h"
#include "toolchain/ToolchainArguments.h"

#include <algorithm>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * Finish a project that states it builds nothing.
 *
 * Nothing of the system is asked for, since nothing is compiled. The
 * database is emptied so an editor stops reading the commands of targets the
 * project no longer has.
 */
std::expected<void, int> finishWithNothingToBuild(const std::filesystem::path& databaseDirectory, const Compile::BuildProfile profile)
{
    const auto written = Compile::writeCompilationDatabase(databaseDirectory, {});
    if (! written.has_value()) {
        std::cerr << renderCompilationDatabaseFailure(written.error());
        return std::unexpected(1);
    }
    std::cerr << renderBuildFinished(profile);
    return {};
}

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
 * The commands that archive the libraries among @p targets, with the
 * archiver the compiler names. An archiver that cannot be found is reported;
 * without a library, none is looked for.
 */
std::expected<std::vector<Compile::ArchiveCommand>, int> planArchives(const RuntimeEnvironment& env,
                                                                      const Compile::BuildSettings& settings,
                                                                      const std::vector<Project::TargetSources>& targets)
{
    const auto isLibrary = [](const Project::TargetSources& each) {
        return each.target.kind == Project::TargetKind::Library;
    };
    if (std::ranges::none_of(targets, isLibrary)) {
        return std::vector<Compile::ArchiveCommand>{};
    }
    const auto archiver = Toolchain::findArchiver(settings.compiler, env.systemSearchPaths);
    if (! archiver.has_value()) {
        const auto& named = archiver.error().named;
        std::cerr << (named.has_value() ? renderArchiverNotFound(*named, settings.compiler) : renderArchiverNotNamed(settings.compiler));
        return std::unexpected(1);
    }
    return Compile::planArchiveCommands(settings, *archiver, targets);
}

/**
 * The compiler the build of @p project uses, named on standard error with
 * where it came from. When none can be used, the reason is reported instead.
 */
std::expected<ChosenCompiler, int> findCompiler(const RuntimeEnvironment& env, const Project::LoadedProject& project)
{
    auto chosen = chooseCompiler(env, project.manifest.toolchain);
    if (! chosen.has_value()) {
        std::cerr << chosen.error();
        return std::unexpected(1);
    }
    std::cerr << chosen->announcement;
    return std::move(*chosen);
}

/**
 * What @p compiler is given to compile and link against the macOS SDK: only
 * the compiler of an installed toolchain, on darwin, needs it. Reports an
 * SDK that cannot be found.
 */
std::expected<std::vector<std::string>, int> sdkArgumentsFor(const RuntimeEnvironment& env, const ChosenCompiler& compiler)
{
    if (! compiler.installed) {
        return std::vector<std::string>{};
    }
    const std::optional<Toolchain::HostEnvironment> host = Toolchain::hostEnvironment();
    const std::string_view platform = host.has_value() ? host->platform : std::string_view{};
    auto arguments = Toolchain::sdkArguments(platform, env.sdkRoot, Toolchain::askXcrunForSdkPath);
    if (! arguments.has_value()) {
        std::cerr << renderSdkNotFound(compiler.compiler);
        return std::unexpected(1);
    }
    return std::move(*arguments);
}

/**
 * @p arguments with @p sdk added to both its lists.
 */
Toolchain::ToolchainArguments withSdk(Toolchain::ToolchainArguments arguments, const std::vector<std::string>& sdk)
{
    arguments.compile.insert(arguments.compile.end(), sdk.begin(), sdk.end());
    arguments.link.insert(arguments.link.end(), sdk.begin(), sdk.end());
    return arguments;
}

/**
 * The settings to build @p project with: the compiler it uses, named on
 * standard error with where it came from, and what that compiler is given.
 * The macOS SDK is asked for before the compiler is identified.
 */
std::expected<Compile::BuildSettings, int> settingsFor(const RuntimeEnvironment& env,
                                                       const Project::LoadedProject& project,
                                                       const Compile::BuildProfile profile,
                                                       const std::filesystem::path& buildDirectory)
{
    const auto compiler = findCompiler(env, project);
    if (! compiler.has_value()) {
        return std::unexpected(compiler.error());
    }
    const auto sdk = sdkArgumentsFor(env, *compiler);
    if (! sdk.has_value()) {
        return std::unexpected(sdk.error());
    }
    return Compile::BuildSettings{
        .projectRoot = project.root,
        .buildDirectory = buildDirectory,
        .compiler = compiler->compiler,
        .driver = Compile::CompilerDriver{ Toolchain::identifyCompiler(compiler->compiler) },
        .standard = project.manifest.package.standard,
        .profile = profile,
        .runtimeLibraryDirectories = compiler->runtimeLibraryDirectories,
        .arguments = withSdk(compiler->arguments, *sdk)
    };
}

/**
 * The names of @p targets, in their order.
 */
std::vector<std::string> namesOf(const std::vector<Project::Target>& targets)
{
    std::vector<std::string> names;
    names.reserve(targets.size());
    for (const Project::Target& target : targets) {
        names.push_back(target.name);
    }
    return names;
}

/**
 * Whether @p name is the name of one of @p targets.
 */
bool namesOneOf(const std::vector<Project::Target>& targets, const std::string& name)
{
    return std::ranges::find(targets, name, &Project::Target::name) != targets.end();
}

/**
 * The targets a build of @p target needs among those of @p project, or every
 * target when it is not given. A name the project has no target of is
 * reported, with the targets there are.
 */
std::expected<std::vector<Project::Target>, int> chooseTargets(const Project::LoadedProject& project,
                                                               const std::vector<Project::Target>& targets,
                                                               const std::optional<std::string>& target)
{
    if (! target.has_value()) {
        return targets;
    }
    auto needed = Project::targetsToBuild(targets, *target);
    if (! needed.has_value()) {
        std::cerr << renderNoTargetNamed(project.root, *target, namesOf(targets));
        return std::unexpected(1);
    }
    return std::move(*needed);
}

/**
 * The sources of the targets among @p chosen.
 */
std::vector<Project::TargetSources> sourcesOf(const std::vector<Project::TargetSources>& sources, const std::vector<Project::Target>& chosen)
{
    std::vector<Project::TargetSources> kept;
    std::ranges::copy_if(sources, std::back_inserter(kept), [&](const Project::TargetSources& each) {
        return namesOneOf(chosen, each.target.name);
    });
    return kept;
}

/**
 * Compile, archive and link @p targets, as many steps at once as the system
 * has processors for, reporting each step as it runs. A step whose inputs
 * have not changed since it last succeeded is not run.
 */
std::expected<void, int> runBuild(const RuntimeEnvironment& env,
                                  const Compile::BuildSettings& settings,
                                  const std::vector<Project::TargetSources>& targets)
{
    const auto archives = planArchives(env, settings, targets);
    if (! archives.has_value()) {
        return std::unexpected(archives.error());
    }
    Build::ProgramStepRunner runner;
    Build::RecordedStepHistory history{ settings.buildDirectory };
    StreamBuildReporter reporter{ std::cerr, standardErrorIsTerminal() };
    const auto steps = Build::buildSteps(Compile::planCompileCommands(settings, targets), *archives, Compile::planLinkCommands(settings, targets));
    const auto built = Build::runSteps(steps, runner, history, reporter, Build::availableParallelism());
    if (! built.has_value()) {
        std::cerr << renderStepFailures(built.error());
        return std::unexpected(1);
    }
    std::cerr << renderBuildFinished(settings.profile);
    return {};
}

/**
 * The targets of @p project. A project that declares none, or more than one
 * library, is reported.
 */
std::expected<std::vector<Project::Target>, int> resolveBuildTargets(const Project::LoadedProject& project)
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

}  // anonymous namespace

Compile::BuildProfile requestedProfile(const ParsedOptions& options)
{
    const auto option = options.named.find(std::string{ ReleaseOption });
    if (option == options.named.end()) {
        return Compile::BuildProfile::Debug;
    }
    const bool* const release = std::get_if<bool>(&option->second);
    return ((release != nullptr) && *release) ? Compile::BuildProfile::Release : Compile::BuildProfile::Debug;
}

std::optional<std::string> requestedName(const ParsedOptions& options, const std::string_view option)
{
    const auto found = options.named.find(std::string{ option });
    if (found == options.named.end()) {
        return std::nullopt;
    }
    const std::string* const name = std::get_if<std::string>(&found->second);
    return (name != nullptr) ? std::optional<std::string>{ *name } : std::nullopt;
}

std::expected<std::filesystem::path, int> buildProject(const RuntimeEnvironment& env,
                                                       const Project::LoadedProject& project,
                                                       const Compile::BuildProfile profile,
                                                       const std::optional<std::string>& target)
{
    const auto targets = resolveBuildTargets(project);
    if (! targets.has_value()) {
        return std::unexpected(targets.error());
    }

    // The name is checked against the project before the system is asked
    // for anything, as the other mistakes in the project are.
    const auto chosen = chooseTargets(project, *targets, target);
    if (! chosen.has_value()) {
        return std::unexpected(chosen.error());
    }

    std::filesystem::path buildDirectory{ Compile::buildDirectoryOf(profile) };
    if (targets->empty()) {
        const auto finished = finishWithNothingToBuild(project.root / buildDirectory, profile);
        if (! finished.has_value()) {
            return std::unexpected(finished.error());
        }
        return buildDirectory;
    }

    const auto sources = sourcesToBuild(project, *targets);
    if (! sources.has_value()) {
        return std::unexpected(sources.error());
    }

    const auto settingsResult = settingsFor(env, project, profile, buildDirectory);
    if (! settingsResult.has_value()) {
        return std::unexpected(settingsResult.error());
    }
    const Compile::BuildSettings& settings = *settingsResult;
    const auto compiles = Compile::planCompileCommands(settings, *sources);
    const auto written = Compile::writeCompilationDatabase(project.root / buildDirectory, compiles);
    if (! written.has_value()) {
        std::cerr << renderCompilationDatabaseFailure(written.error());
        return std::unexpected(1);
    }

    // The database is written before the build stops for a reason below, so
    // an editor reads the commands whether or not they can run.
    if (! settings.driver.standardOption(settings.standard).has_value()) {
        std::cerr << renderUnsupportedStandard(settings.compiler, settings.standard);
        return std::unexpected(1);
    }

    // The database covers every target, so it does not change with the one
    // asked for; only the targets chosen are compiled and linked.
    const auto built = runBuild(env, settings, sourcesOf(*sources, *chosen));
    if (! built.has_value()) {
        return std::unexpected(built.error());
    }
    return buildDirectory;
}

}  // namespace scrap::Command
