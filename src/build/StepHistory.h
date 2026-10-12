#pragma once

#include "build/BuildStep.h"

#include <filesystem>
#include <optional>

namespace scrap::Build {

/**
 * @brief What the steps of earlier builds did, and whether a step needs to
 *        run again.
 *
 * May be called from several threads at once, each for a step of its own.
 */
class StepHistory {
public:
    virtual ~StepHistory();
    StepHistory(const StepHistory&) = default;
    StepHistory& operator=(const StepHistory&) = default;
    StepHistory(StepHistory&&) = default;
    StepHistory& operator=(StepHistory&&) = default;

    /**
     * @brief Whether running @p step again would write nothing different.
     */
    [[nodiscard]] virtual bool upToDate(const BuildStep& step) = 0;

    /**
     * @brief Forget what @p step did before, as it is about to run.
     */
    virtual void starting(const BuildStep& step) = 0;

    /**
     * @brief Remember what @p step read, once it has succeeded.
     */
    virtual void succeeded(const BuildStep& step) = 0;

protected:
    StepHistory() = default;
};

/**
 * @brief Keeps what each step read in a record file of its own, below the
 *        records/ directory of the build directory.
 *
 * The record of a step lies at the path of its output below the build
 * directory, taken below records/. A step is up to date when its output is
 * there, its record can be read, and the record matches what the step reads
 * now, as observeStep() sees it. A step whose output lies outside the build
 * directory, or below records/, keeps no record, so it always runs.
 *
 * The record is removed before the step runs and written only once it has
 * succeeded, so a step that failed or was stopped runs again. A record that
 * cannot be removed or written leaves the step to run again next time.
 */
class RecordedStepHistory final : public StepHistory {
public:
    /**
     * @param buildDirectory The build directory, relative to the directory
     *        the steps run in.
     */
    explicit RecordedStepHistory(std::filesystem::path buildDirectory);

    [[nodiscard]] bool upToDate(const BuildStep& step) override;
    void starting(const BuildStep& step) override;
    void succeeded(const BuildStep& step) override;

private:
    /**
     * Where the record of @p step lies, or nothing for a step that keeps none.
     */
    [[nodiscard]] std::optional<std::filesystem::path> recordFile(const BuildStep& step) const;

    std::filesystem::path _buildDirectory;
};

}  // namespace scrap::Build
