#include "command/ProjectBuild.h"

#include "build/BuildSteps.h"
#include "build/Parallelism.h"
#include "build/StepHistory.h"
#include "build/StepRunner.h"
#include "command/BuildProgress.h"
#include "command/ParsedOptions.h"
#include "command/ProjectCompiles.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "compile/ArchiveCommand.h"
#include "compile/BuildProfile.h"
#include "compile/CompilationDatabase.h"
#include "compile/CompilePlanner.h"
#include "project/Manifest.h"
#include "project/ProjectLoader.h"
#include "project/SourceCollector.h"
#include "project/TargetResolver.h"
#include "toolchain/SystemArchiver.h"

#include <algorithm>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
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
    const auto archiver = Toolchain::findArchiver(settings.cxx.compiler, env.systemSearchPaths);
    if (! archiver.has_value()) {
        const auto& named = archiver.error().named;
        std::cerr << (named.has_value() ? renderArchiverNotFound(*named, settings.cxx.compiler) : renderArchiverNotNamed(settings.cxx.compiler));
        return std::unexpected(1);
    }
    return Compile::planArchiveCommands(settings, *archiver, targets);
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
 * Compile, archive and link @p targets, @p parallelism steps at once,
 * reporting each step as it runs. A step whose inputs have not changed since
 * it last succeeded is not run.
 */
std::expected<void, int> runBuild(const RuntimeEnvironment& env,
                                  const Compile::BuildSettings& settings,
                                  const std::vector<Project::TargetSources>& targets,
                                  const unsigned parallelism)
{
    const auto archives = planArchives(env, settings, targets);
    if (! archives.has_value()) {
        return std::unexpected(archives.error());
    }
    Build::ProgramStepRunner runner;
    Build::RecordedStepHistory history{ settings.buildDirectory };
    StreamBuildReporter reporter{ std::cerr, standardErrorIsTerminal() };
    const auto steps = Build::buildSteps(Compile::planCompileCommands(settings, targets), *archives, Compile::planLinkCommands(settings, targets));
    const auto built = Build::runSteps(steps, runner, history, reporter, parallelism);
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
    return isGiven(options, ReleaseOption) ? Compile::BuildProfile::Release : Compile::BuildProfile::Debug;
}

std::optional<unsigned> requestedJobs(const ParsedOptions& options)
{
    const auto found = options.named.find(std::string{ JobsOption });
    if (found == options.named.end()) {
        return std::nullopt;
    }
    const std::int64_t asked = std::get<std::int64_t>(found->second);
    return static_cast<unsigned>(std::min<std::int64_t>(asked, std::numeric_limits<unsigned>::max()));
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
                                                       const std::optional<std::string>& target,
                                                       const std::optional<unsigned>& jobs)
{
    const auto targets = resolveProjectTargets(project);
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

    const auto compiles = planProjectCompiles(env, project, profile, *targets);
    if (! compiles.has_value()) {
        return std::unexpected(compiles.error());
    }

    // The database covers every target, so it does not change with the one
    // asked for; only the targets chosen are compiled and linked.
    const auto built = runBuild(env, compiles->settings, sourcesOf(compiles->sources, *chosen), jobs.value_or(Build::availableParallelism()));
    if (! built.has_value()) {
        return std::unexpected(built.error());
    }
    return buildDirectory;
}

}  // namespace scrap::Command
