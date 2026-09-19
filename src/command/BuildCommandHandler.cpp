#include "command/BuildCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "compile/CompilationDatabase.h"
#include "compile/CompilePlanner.h"
#include "compile/CompilerDriver.h"
#include "project/ProjectLoader.h"
#include "project/SourceCollector.h"
#include "project/TargetResolver.h"
#include "toolchain/CompilerIdentity.h"
#include "toolchain/SystemCompiler.h"

#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>

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
    std::cout << "Using the system compiler '" << printablePath(compiler->path) << "' ("
              << describeOrigin(compiler->origin) << ")\n";

    // Written for a project that builds nothing as well, so an editor stops
    // reading the commands of targets the project no longer has.
    const Compile::BuildSettings settings{
        .projectRoot = project->root,
        .buildDirectory = std::filesystem::path{Compile::DebugBuildDirectory},
        .compiler = compiler->path,
        .driver = Compile::CompilerDriver{Toolchain::identifyCompiler(compiler->path)},
        .standard = project->manifest.package.standard};
    const auto commands = Compile::planCompileCommands(settings, *sources);
    const auto written = Compile::writeCompilationDatabase(project->root / settings.buildDirectory, commands);
    if (! written.has_value()) {
        std::cerr << renderCompilationDatabaseFailure(written.error());
        return 1;
    }

    // Placeholder output until the build compiles the project.
    std::cout << "build: not yet implemented\n";
    return 0;
}

}  // namespace scrap::Command
