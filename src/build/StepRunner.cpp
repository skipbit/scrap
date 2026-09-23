#include "build/StepRunner.h"

#include "build/BuildStep.h"
#include "process/Subprocess.h"

#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>

namespace scrap::Build {

StepRunner::~StepRunner() = default;

StepResult ProgramStepRunner::run(const BuildStep& step)
{
    const std::filesystem::path outputDirectory = (step.directory / step.output).parent_path();
    std::error_code ec;
    std::filesystem::create_directories(outputDirectory, ec);
    if (ec) {
        return StepResult{ .output = {},
                           .failure = StepFailure{
                               .kind = StepFailureKind::CannotCreateDirectory, .path = outputDirectory, .code = ec, .status = 0 } };
    }

    auto completion = Process::runProgram(step.arguments, step.directory, Process::OutputCapture::Combined);
    if (! completion.has_value()) {
        return StepResult{ .output = {},
                           .failure = StepFailure{ .kind = StepFailureKind::CannotStart,
                                                   .path = step.arguments.empty() ? std::filesystem::path{}
                                                                                  : std::filesystem::path{ step.arguments.front() },
                                                   .code = completion.error(),
                                                   .status = 0 } };
    }

    StepResult result{ .output = std::move(completion->output), .failure = std::nullopt };
    if (const std::optional<int> signal = completion->signal; signal.has_value()) {
        result.failure = StepFailure{ .kind = StepFailureKind::Signalled, .path = {}, .code = {}, .status = *signal };
    } else if (completion->exitCode != 0) {
        result.failure
            = StepFailure{ .kind = StepFailureKind::Exited, .path = {}, .code = {}, .status = completion->exitCode.value_or(-1) };
    }
    return result;
}

}  // namespace scrap::Build
