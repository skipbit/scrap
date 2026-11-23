#pragma once

#include "check/ParallelAnalysis.h"
#include "compile/BuildProfile.h"
#include "tool/ExternalTool.h"

#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Command {

/**
 * @brief Writes what a check is doing, a line per file:
 *
 *         Checking src/main.cpp
 *
 * What the tool wrote for a file is written once the file has ended; with
 * files analyzed at once, other lines may come between the two. The colour
 * the tool wrote is kept or taken out as printableOutput() does.
 */
class StreamAnalysisReporter final : public Check::AnalysisReporter {
public:
    /**
     * @param out Where the progress and the tool's output go.
     * @param keepColor Whether to leave the colour the tool wrote.
     */
    StreamAnalysisReporter(std::ostream& out, bool keepColor);

    void started(const std::filesystem::path& file) override;
    void finished(const std::filesystem::path& file, std::string_view output) override;

private:
    std::ostream* _out;
    bool _keepColor;
};

/**
 * @brief The line that ends a check with the commands of @p profile in which
 *        no file failed.
 */
[[nodiscard]] std::string renderCheckFinished(Compile::BuildProfile profile);

/**
 * @brief Name the files whose analysis failed, and what to do next.
 *
 * What the tool wrote for each file was written as the file ended, so only
 * the file is named, with the signal that stopped the tool when one did, or
 * its exit status when it wrote nothing. A tool that could not be started is
 * named once, however many files it failed.
 *
 * @param tool The tool run.
 * @param failures The files whose analysis failed; not empty.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderAnalysisFailures(const Tool::ExternalTool& tool, const std::vector<Check::FailedAnalysis>& failures);

}  // namespace scrap::Command
