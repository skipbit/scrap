#pragma once

#include "build/BuildStep.h"

namespace scrap::Build {

/**
 * @brief Carries out one step of a build.
 *
 * The order steps run in, and when a build stops, belong to the caller; this
 * is only how a single step is done.
 */
class StepRunner {
public:
    virtual ~StepRunner();
    StepRunner(const StepRunner&) = default;
    StepRunner& operator=(const StepRunner&) = default;
    StepRunner(StepRunner&&) = default;
    StepRunner& operator=(StepRunner&&) = default;

    /**
     * @brief Carry out @p step and report what it came to.
     */
    [[nodiscard]] virtual auto run(const BuildStep& step) -> StepResult = 0;

protected:
    StepRunner() = default;
};

/**
 * @brief Carries out a step by running its program.
 *
 * The directory the output goes in is created first, since a compiler does
 * not create it. Both output streams are read back together, so what the
 * program wrote reaches the reporter in the order it was written.
 */
class ProgramStepRunner final : public StepRunner {
public:
    [[nodiscard]] auto run(const BuildStep& step) -> StepResult override;
};

}  // namespace scrap::Build
