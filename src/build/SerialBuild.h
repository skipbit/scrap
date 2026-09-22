#pragma once

#include "build/BuildReporter.h"
#include "build/BuildStep.h"
#include "build/StepRunner.h"
#include "compile/CompileCommand.h"
#include "compile/LinkCommand.h"

#include <expected>  // IWYU pragma: keep
#include <vector>

namespace scrap::Build {

/**
 * @brief The step that failed, and why.
 */
struct FailedStep {
    BuildStep step;
    StepFailure failure;
};

/**
 * @brief The steps a build takes: every compilation, then every link.
 *
 * A link reads the object files the compilations write, so no link comes
 * before them.
 *
 * @param compiles The compile commands, in the order to run them.
 * @param links The link commands, in the order to run them.
 */
[[nodiscard]] auto buildSteps(const std::vector<Compile::CompileCommand>& compiles,
                              const std::vector<Compile::LinkCommand>& links) -> std::vector<BuildStep>;

/**
 * @brief Run @p steps one after another, stopping at the first that fails.
 *
 * The reporter hears of each step before it runs and again once it has
 * ended, with what its program wrote. A step that fails is the last one run:
 * the steps after it would compile or link on top of an error already
 * reported.
 *
 * @param steps The steps to run, in order.
 * @param runner Carries out each step.
 * @param reporter Told of each step as it runs.
 * @return Nothing when every step succeeded, or the step that failed.
 */
[[nodiscard]] auto runSerially(const std::vector<BuildStep>& steps,
                               StepRunner& runner,
                               BuildReporter& reporter) -> std::expected<void, FailedStep>;

}  // namespace scrap::Build
