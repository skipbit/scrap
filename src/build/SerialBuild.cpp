#include "build/SerialBuild.h"

#include "build/BuildReporter.h"
#include "build/BuildStep.h"
#include "build/StepRunner.h"
#include "compile/CompileCommand.h"
#include "compile/LinkCommand.h"

#include <expected>  // IWYU pragma: keep
#include <vector>

namespace scrap::Build {

std::vector<BuildStep> buildSteps(const std::vector<Compile::CompileCommand>& compiles,
                                  const std::vector<Compile::LinkCommand>& links)
{
    std::vector<BuildStep> steps;
    steps.reserve(compiles.size() + links.size());
    for (const Compile::CompileCommand& command : compiles) {
        steps.push_back(BuildStep{ .kind = StepKind::Compile, .target = command.target, .subject = command.file, .directory = command.directory, .output = command.output, .arguments = command.arguments });
    }
    for (const Compile::LinkCommand& command : links) {
        steps.push_back(BuildStep{ .kind = StepKind::Link, .target = command.target, .subject = command.output, .directory = command.directory, .output = command.output, .arguments = command.arguments });
    }
    return steps;
}

std::expected<void, FailedStep> runSerially(const std::vector<BuildStep>& steps, StepRunner& runner, BuildReporter& reporter)
{
    for (const BuildStep& step : steps) {
        reporter.started(step);
        const StepResult result = runner.run(step);
        reporter.finished(step, result.output);
        if (result.failure.has_value()) {
            return std::unexpected(FailedStep{ .step = step, .failure = *result.failure });
        }
    }
    return {};
}

}  // namespace scrap::Build
