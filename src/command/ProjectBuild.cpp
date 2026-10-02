#include "command/ProjectBuild.h"

#include "build/BuildSteps.h"
#include "build/Parallelism.h"
#include "build/StepRunner.h"
#include "command/BuildProgress.h"
#include "command/ParsedOptions.h"
#include "command/PrintableText.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "compile/BuildProfile.h"
#include "compile/CompilationDatabase.h"
#include "compile/CompileCommand.h"
#include "compile/CompilePlanner.h"
#include "compile/CompilerDriver.h"
#include "project/Manifest.h"
#include "project/ProjectLoader.h"
#include "project/SourceCollector.h"
#include "project/TargetResolver.h"
#include "toolchain/CompilerIdentity.h"
#include "toolchain/SystemCompiler.h"

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
 * How the compiler in use was arrived at, in the words the output uses.
 */
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
 * The first library among @p targets, or nothing when they are all
 * executables.
 */
const Project::Target* libraryAmong(const std::vector<Project::Target>& targets)
{
    for (const Project::Target& target : targets) {
        if (target.kind == Project::TargetKind::Library) {
            return &target;
        }
    }
    return nullptr;
}

/**
 * The system compiler the build uses, named on standard error with how it was
 * found. When none can be used, the reason is reported instead.
 */
std::expected<Toolchain::SystemCompiler, int> findCompiler(const RuntimeEnvironment& env)
{
    auto compiler = Toolchain::detectSystemCompiler(env.preferredCompiler, env.systemSearchPaths);
    if (! compiler.has_value()) {
        if (compiler.error().requested.empty()) {
            std::cerr << renderNoCompilerFound();
        } else {
            std::cerr << renderUnusableCompilerRequest(compiler.error().requested);
        }
        return std::unexpected(1);
    }
    std::cerr << "Using the system compiler '" << printablePath(compiler->path) << "' (" << describeOrigin(compiler->origin) << ")\n";
    return std::move(*compiler);
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
 * Compile and link what @p compiles and the targets state, as many steps at
 * once as the system has processors for, reporting each step as it runs.
 */
std::expected<void, int> runBuild(const Compile::BuildSettings& settings,
                                  const std::vector<Project::TargetSources>& targets,
                                  const std::vector<Compile::CompileCommand>& compiles)
{
    Build::ProgramStepRunner runner;
    StreamBuildReporter reporter{ std::cerr, standardErrorIsTerminal() };
    const auto built = Build::runSteps(Build::buildSteps(compiles, {}, Compile::planLinkCommands(settings, targets)), runner, reporter, Build::availableParallelism());
    if (! built.has_value()) {
        std::cerr << renderStepFailures(built.error());
        return std::unexpected(1);
    }
    std::cerr << renderBuildFinished(settings.profile);
    return {};
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
    // An empty declaration states that the project builds nothing, which is a
    // different answer from finding nothing where no declaration was written.
    const auto targets = Project::resolveTargets(project.root, project.manifest);
    if (targets.empty() && (! project.manifest.declaresTargets)) {
        std::cerr << renderNoTargetToBuild(project.root);
        return std::unexpected(1);
    }

    // The name is checked against the project before the system is asked
    // for anything, as the other mistakes in the project are.
    const auto chosen = chooseTargets(project, targets, target);
    if (! chosen.has_value()) {
        return std::unexpected(chosen.error());
    }

    std::filesystem::path buildDirectory{ Compile::buildDirectoryOf(profile) };
    if (targets.empty()) {
        const auto finished = finishWithNothingToBuild(project.root / buildDirectory, profile);
        if (! finished.has_value()) {
            return std::unexpected(finished.error());
        }
        return buildDirectory;
    }

    const auto sources = Project::collectSources(project.root, targets);
    if (! sources.has_value()) {
        std::cerr << renderSourceScanFailure(sources.error());
        return std::unexpected(1);
    }

    const auto compiler = findCompiler(env);
    if (! compiler.has_value()) {
        return std::unexpected(compiler.error());
    }

    const Compile::BuildSettings settings{
        .projectRoot = project.root,
        .buildDirectory = buildDirectory,
        .compiler = compiler->path,
        .driver = Compile::CompilerDriver{ Toolchain::identifyCompiler(compiler->path) },
        .standard = project.manifest.package.standard,
        .profile = profile
    };
    const auto compiles = Compile::planCompileCommands(settings, *sources);
    const auto written = Compile::writeCompilationDatabase(project.root / buildDirectory, compiles);
    if (! written.has_value()) {
        std::cerr << renderCompilationDatabaseFailure(written.error());
        return std::unexpected(1);
    }

    // The database is written before the build stops for either reason
    // below, so an editor reads the commands whether or not they can run.
    if (const Project::Target* library = libraryAmong(targets); library != nullptr) {
        std::cerr << renderLibraryNotBuilt(library->name);
        return std::unexpected(1);
    }
    if (! settings.driver.standardOption(settings.standard).has_value()) {
        std::cerr << renderUnsupportedStandard(compiler->path, settings.standard);
        return std::unexpected(1);
    }

    // The database covers every target, so it does not change with the one
    // asked for; only the targets chosen are compiled and linked.
    const auto chosenSources = sourcesOf(*sources, *chosen);
    const auto built = runBuild(settings, chosenSources, Compile::planCompileCommands(settings, chosenSources));
    if (! built.has_value()) {
        return std::unexpected(built.error());
    }
    return buildDirectory;
}

}  // namespace scrap::Command
