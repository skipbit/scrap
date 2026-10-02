#include "command/RunCommandHandler.h"

#include "command/BuildProgress.h"
#include "command/InvocationContext.h"
#include "command/ProjectArgument.h"
#include "command/ProjectBuild.h"
#include "command/ProjectDiagnostic.h"
#include "compile/CompilePlanner.h"
#include "process/Subprocess.h"
#include "project/Manifest.h"
#include "project/TargetResolver.h"

#include <algorithm>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace scrap::Command {

namespace {

/// Added to the number of the signal that stopped the program, as a shell does.
constexpr int SignalExitCodeBase = 128;

/**
 * The names of the executable targets among @p targets, in their order.
 */
std::vector<std::string> executablesAmong(const std::vector<Project::Target>& targets)
{
    std::vector<std::string> names;
    for (const Project::Target& target : targets) {
        if (target.kind == Project::TargetKind::Executable) {
            names.push_back(target.name);
        }
    }
    return names;
}

}  // anonymous namespace

int RunCommandHandler::execute(const InvocationContext& ctx)
{
    const auto project = loadProjectAt(ctx);
    if (! project.has_value()) {
        std::cerr << renderProjectArgumentError(project.error());
        return 1;
    }

    // Which executable to run is settled before building, so a project that
    // has none, or more than one and no choice among them, is not built for
    // nothing.
    const auto executables = executablesAmong(Project::resolveTargets(project->root, project->manifest));
    const std::optional<std::string> requested = requestedName(ctx.options, BinOption);
    if (requested.has_value() && (std::ranges::find(executables, *requested) == executables.end())) {
        std::cerr << renderNoExecutableNamed(project->root, *requested, executables);
        return 1;
    }
    if (executables.empty()) {
        std::cerr << renderNoExecutableToRun(project->root);
        return 1;
    }
    if ((! requested.has_value()) && (executables.size() > 1)) {
        std::cerr << renderSeveralExecutablesToRun(project->root, executables);
        return 1;
    }
    const std::string name = requested.value_or(executables.front());

    const auto built = buildProject(*ctx.env, *project, requestedProfile(ctx.options), name);
    if (! built.has_value()) {
        return BuildFailedExitCode;
    }

    const std::filesystem::path executable = Compile::executableFile(*built, name);
    const std::filesystem::path program = project->root / executable;
    std::cerr << renderRunning(name, executable);

    std::vector<std::string> arguments{ program.string() };
    arguments.insert(arguments.end(), ctx.options.trailing.begin(), ctx.options.trailing.end());
    const auto completion = Process::runProgram(arguments,
                                                { .workingDirectory = {},
                                                  .capture = Process::OutputCapture::Terminal,
                                                  .group = Process::ProcessGroup::Caller,
                                                  .timeout = std::nullopt,
                                                  .outputLimit = std::nullopt });
    if (! completion.has_value()) {
        std::cerr << renderExecutableNotStarted(program, completion.error());
        return 1;
    }
    if (const std::optional<int> signal = completion->signal; signal.has_value()) {
        // A shell stops a loop or a script on an interrupt only when the
        // command itself ends by it. The signal takes what scrap was started
        // with, so one it ignores leaves scrap to return the code below.
        if ((*signal == SIGINT) || (*signal == SIGQUIT)) {  // NOLINT(misc-include-cleaner) - SIGQUIT is provided by <csignal> on POSIX
            // Returning from the raise means the signal is ignored.
            (void)std::raise(*signal);
        }
        return SignalExitCodeBase + *signal;
    }
    return completion->exitCode.value_or(1);
}

}  // namespace scrap::Command
