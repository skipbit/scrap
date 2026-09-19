#include "command/BuildCommandHandler.h"

#include "build/SerialBuild.h"
#include "build/StepRunner.h"
#include "command/BuildProgress.h"
#include "command/InvocationContext.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
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

#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * How the compiler in use was arrived at, in the words the output uses.
 */
auto describeOrigin(const Toolchain::CompilerOrigin origin) -> std::string_view
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
 * The path argument taken against the working directory, or the working
 * directory itself.
 */
auto startDirectory(const InvocationContext& ctx) -> std::filesystem::path
{
    if (ctx.options.positional.empty()) {
        return ctx.env->workingDirectory;
    }
    return ctx.env->workingDirectory / ctx.options.positional.front();
}

/**
 * Finish a project that states it builds nothing.
 *
 * Nothing of the system is asked for, since nothing is compiled. The
 * database is emptied so an editor stops reading the commands of targets the
 * project no longer has.
 */
auto finishWithNothingToBuild(const std::filesystem::path& databaseDirectory) -> int
{
    const auto written = Compile::writeCompilationDatabase(databaseDirectory, {});
    if (! written.has_value()) {
        std::cerr << renderCompilationDatabaseFailure(written.error());
        return 1;
    }
    std::cerr << renderBuildFinished();
    return 0;
}

/**
 * The first library among @p targets, or nothing when they are all
 * executables.
 */
auto libraryAmong(const std::vector<Project::Target>& targets) -> const Project::Target*
{
    for (const Project::Target& target : targets) {
        if (target.kind == Project::TargetKind::Library) {
            return &target;
        }
    }
    return nullptr;
}

/**
 * Compile and link what @p compiles and the targets state, reporting each
 * step as it runs.
 */
auto runBuild(const Compile::BuildSettings& settings,
              const std::vector<Project::TargetSources>& targets,
              const std::vector<Compile::CompileCommand>& compiles) -> int
{
    Build::ProgramStepRunner runner;
    StreamBuildReporter reporter{std::cerr, standardErrorIsTerminal()};
    const auto built =
        Build::runSerially(Build::buildSteps(compiles, Compile::planLinkCommands(settings, targets)), runner, reporter);
    if (! built.has_value()) {
        std::cerr << renderStepFailure(built.error());
        return 1;
    }
    std::cerr << renderBuildFinished();
    return 0;
}

}  // anonymous namespace

auto BuildCommandHandler::execute(const InvocationContext& ctx) -> int
{
    // An explicitly empty argument is usually an unset variable, so it is
    // reported as an error instead of standing for the working directory.
    if (! ctx.options.positional.empty() && ctx.options.positional.front().empty()) {
        std::cerr << renderEmptyPathArgument();
        return 1;
    }

    const auto project = Project::loadProject(startDirectory(ctx));
    if (! project.has_value()) {
        std::cerr << renderProjectError(project.error());
        return 1;
    }

    // An empty declaration states that the project builds nothing, which is a
    // different answer from finding nothing where no declaration was written.
    const auto targets = Project::resolveTargets(project->root, project->manifest);
    if (targets.empty() && ! project->manifest.declaresTargets) {
        std::cerr << renderNoTargetToBuild(project->root);
        return 1;
    }

    const std::filesystem::path buildDirectory{Compile::DebugBuildDirectory};
    if (targets.empty()) {
        return finishWithNothingToBuild(project->root / buildDirectory);
    }

    const auto sources = Project::collectSources(project->root, targets);
    if (! sources.has_value()) {
        std::cerr << renderSourceScanFailure(sources.error());
        return 1;
    }

    const auto compiler = Toolchain::detectSystemCompiler(ctx.env->preferredCompiler, ctx.env->systemSearchPaths);
    if (! compiler.has_value()) {
        if (compiler.error().requested.empty()) {
            std::cerr << renderNoCompilerFound();
        } else {
            std::cerr << renderUnusableCompilerRequest(compiler.error().requested);
        }
        return 1;
    }
    std::cerr << "Using the system compiler '" << printablePath(compiler->path) << "' ("
              << describeOrigin(compiler->origin) << ")\n";

    const Compile::BuildSettings settings{
        .projectRoot = project->root,
        .buildDirectory = buildDirectory,
        .compiler = compiler->path,
        .driver = Compile::CompilerDriver{Toolchain::identifyCompiler(compiler->path)},
        .standard = project->manifest.package.standard};
    const auto compiles = Compile::planCompileCommands(settings, *sources);
    const auto written = Compile::writeCompilationDatabase(project->root / buildDirectory, compiles);
    if (! written.has_value()) {
        std::cerr << renderCompilationDatabaseFailure(written.error());
        return 1;
    }

    // The database is written before the build stops for either reason
    // below, so an editor reads the commands whether or not they can run.
    if (const Project::Target* library = libraryAmong(targets); library != nullptr) {
        std::cerr << renderLibraryNotBuilt(library->name);
        return 1;
    }
    if (! settings.driver.standardOption(settings.standard).has_value()) {
        std::cerr << renderUnsupportedStandard(compiler->path, settings.standard);
        return 1;
    }

    return runBuild(settings, *sources, compiles);
}

}  // namespace scrap::Command
