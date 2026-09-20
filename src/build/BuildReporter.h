#pragma once

#include "build/BuildStep.h"

#include <string_view>

namespace scrap::Build {

/**
 * @brief Told of each step as the build runs it.
 */
class BuildReporter {
public:
    virtual ~BuildReporter();
    BuildReporter(const BuildReporter&) = default;
    BuildReporter& operator=(const BuildReporter&) = default;
    BuildReporter(BuildReporter&&) = default;
    BuildReporter& operator=(BuildReporter&&) = default;

    /**
     * @brief @p step is about to run.
     */
    virtual void started(const BuildStep& step) = 0;

    /**
     * @brief @p step has ended, whether or not it succeeded.
     *
     * @param step The step that ran.
     * @param output What its program wrote, which may be empty.
     */
    virtual void finished(const BuildStep& step, std::string_view output) = 0;

protected:
    BuildReporter() = default;
};

}  // namespace scrap::Build
