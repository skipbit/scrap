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
 * @brief The steps a build takes: every compilation, then every link.
 *
 * A link reads the object files the compilations write, so no link comes
 * before them.
 *
 * @param compiles The compile commands, in the order to run them.
 * @param links The link commands, in the order to run them.
 */
[[nodiscard]] std::vector<BuildStep> buildSteps(const std::vector<Compile::CompileCommand>& compiles,
                                                const std::vector<Compile::LinkCommand>& links);

/**
 * @brief Run @p steps, as many at once as @p parallelism allows.
 *
 * Steps start in the order given. A link reads the object files the
 * compilations before it write, so no link starts while a compilation
 * before it is still running.
 *
 * Once a step fails, no further step starts: the steps after it would
 * compile or link on top of an error already reported. The steps already
 * running are waited for, and may fail too.
 *
 * The runner is called from several threads at once. The reporter is told of
 * one step at a time, never from two threads together: of each step as it
 * starts, and again once it has ended, with what its program wrote. What
 * steps wrote therefore reaches it in the order they ended.
 *
 * @param steps The steps to run, in order.
 * @param runner Carries out each step.
 * @param reporter Told of each step as it runs.
 * @param parallelism How many steps may run at once. 0 is taken as 1.
 * @return Nothing when every step succeeded, or the steps that failed, in the
 *         order they ended.
 */
[[nodiscard]] std::expected<void, std::vector<FailedStep>> runSteps(const std::vector<BuildStep>& steps,
                                                                    StepRunner& runner,
                                                                    BuildReporter& reporter,
                                                                    unsigned parallelism);

}  // namespace scrap::Build
