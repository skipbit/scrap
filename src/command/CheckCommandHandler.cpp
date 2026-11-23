#include "command/CheckCommandHandler.h"

#include "build/Parallelism.h"
#include "check/ClangTidy.h"
#include "check/ParallelAnalysis.h"
#include "command/CheckText.h"
#include "command/InvocationContext.h"
#include "command/ProjectBuild.h"
#include "command/ProjectCompiles.h"
#include "command/ProjectPaths.h"
#include "command/ProjectPathsText.h"
#include "command/RuntimeEnvironment.h"
#include "command/ToolLookup.h"
#include "command/ToolOutput.h"
#include "compile/BuildProfile.h"
#include "project/Manifest.h"
#include "project/ProjectLoader.h"
#include "project/SourceCollector.h"
#include "tool/ExternalTool.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace scrap::Command {

namespace {

/// The command as the messages name it.
constexpr std::string_view CommandName = "check";

/**
 * The sources of @p targets, each compiled with the commands for @p profile
 * that are written to compile_commands.json first. A project that states it
 * builds nothing has none, and its database is emptied, as a build empties
 * it, without asking the system for a compiler.
 */
std::expected<std::vector<std::filesystem::path>, int> sourcesToCheck(const RuntimeEnvironment& env,
                                                                      const Project::LoadedProject& project,
                                                                      const Compile::BuildProfile profile,
                                                                      const std::vector<Project::Target>& targets)
{
    if (targets.empty()) {
        const auto written = writeProjectDatabase(project, profile, {});
        if (! written.has_value()) {
            return std::unexpected(written.error());
        }
        return std::vector<std::filesystem::path>{};
    }
    const auto compiles = planProjectCompiles(env, project, profile, targets);
    if (! compiles.has_value()) {
        return std::unexpected(compiles.error());
    }
    std::vector<std::filesystem::path> sources;
    for (const Project::TargetSources& target : compiles->sources) {
        sources.insert(sources.end(), target.sources.begin(), target.sources.end());
    }
    return sources;
}

/**
 * Analyze @p files of @p project with @p tool and the commands for
 * @p profile, @p parallelism at once, and return the exit code.
 */
int checkFiles(const Tool::ExternalTool& tool,
               const Project::LoadedProject& project,
               const Compile::BuildProfile profile,
               const std::vector<std::filesystem::path>& files,
               const unsigned parallelism)
{
    const std::filesystem::path database{ Compile::buildDirectoryOf(profile) };
    const Check::FileAnalyzer analyze = [&](const std::filesystem::path& file) {
        return Check::analyzeFile(tool, project.root, database, file, project.manifest.enabledChecks);
    };
    StreamAnalysisReporter reporter{ std::cerr, standardErrorIsTerminal() };
    const auto failures = Check::analyzeFiles(files, analyze, reporter, parallelism);
    if (! failures.empty()) {
        std::cerr << renderAnalysisFailures(tool, failures);
        return 1;
    }
    std::cerr << renderCheckFinished(profile);
    return 0;
}

}  // anonymous namespace

int CheckCommandHandler::execute(const InvocationContext& ctx)
{
    const auto loaded = loadProjectWithPaths(ctx.env->workingDirectory, ctx.options.positional);
    if (! loaded.has_value()) {
        std::cerr << renderProjectPathsError(loaded.error(), CommandName);
        return 1;
    }
    const Project::LoadedProject& project = loaded->project;
    const Compile::BuildProfile profile = requestedProfile(ctx.options);

    const auto targets = resolveProjectTargets(project);
    if (! targets.has_value()) {
        return targets.error();
    }
    const auto sources = sourcesToCheck(*ctx.env, project, profile, *targets);
    if (! sources.has_value()) {
        return sources.error();
    }
    const std::vector<std::filesystem::path> files = selectRequestedFiles(*sources, loaded->requested, CommandName);
    if (files.empty()) {
        return 0;
    }

    const auto tool = lookUpTool(Check::ClangTidyName, ctx.env->systemSearchPaths);
    if (! tool.has_value()) {
        return tool.error();
    }
    return checkFiles(*tool, project, profile, files, requestedJobs(ctx.options).value_or(Build::availableParallelism()));
}

}  // namespace scrap::Command
