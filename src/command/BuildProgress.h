#pragma once

#include "build/BuildReporter.h"
#include "build/BuildStep.h"

#include <iosfwd>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief Writes what a build is doing, a line per step.
 *
 * Each line names what the step is about, in the form the other commands
 * report their progress in:
 *
 *     Compiling hello (src/main.cpp)
 *       Linking hello (build/debug/bin/hello)
 *
 * What the compiler wrote follows the step it belongs to. The compiler is
 * asked for colour, since a build usually runs in a terminal; where the
 * output is not a terminal the colour is taken back out, so a log holds the
 * diagnostics and not the sequences that colour them.
 */
class StreamBuildReporter final : public Build::BuildReporter {
public:
    /**
     * @param out Where the progress and the compiler's output go.
     * @param keepColor Whether to leave the colour the compiler wrote.
     */
    StreamBuildReporter(std::ostream& out, bool keepColor);

    void started(const Build::BuildStep& step) override;
    void finished(const Build::BuildStep& step, std::string_view output) override;

private:
    std::ostream* out_;
    bool keepColor_;
};

/**
 * @brief The line that ends a debug build that succeeded.
 */
[[nodiscard]] auto renderBuildFinished() -> std::string;

/**
 * @brief @p text with the sequences that colour it taken out.
 *
 * Only the sequences a compiler colours its diagnostics with are removed:
 * the ones that set an attribute, and the one gcc clears the rest of the
 * line with.
 */
[[nodiscard]] auto withoutColor(std::string_view text) -> std::string;

/**
 * @brief Whether standard error goes to a terminal.
 */
[[nodiscard]] auto standardErrorIsTerminal() -> bool;

}  // namespace scrap::Command
