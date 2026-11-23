#include "command/CheckText.h"

#include "check/ParallelAnalysis.h"
#include "command/FixErrorsHint.h"
#include "command/PrintableText.h"
#include "command/ProgressLine.h"
#include "command/ToolOutput.h"
#include "command/ToolText.h"
#include "compile/BuildProfile.h"
#include "tool/ExternalTool.h"

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

/// The line naming @p file, whose analysis by @p tool ended in @p failed.
std::string renderFailedFile(const Tool::ExternalTool& tool, const std::filesystem::path& file, const Tool::ToolFailed& failed)
{
    return renderToolFailureHeading(tool, "error: failed to check '" + printablePath(file) + "'", failed);
}

}  // anonymous namespace

StreamAnalysisReporter::StreamAnalysisReporter(std::ostream& out, const bool keepColor)
    : _out(&out)
    , _keepColor(keepColor)
{
}

void StreamAnalysisReporter::started(const std::filesystem::path& file)
{
    *_out << alignedVerb("Checking") << ' ' << printablePath(file) << '\n';
}

void StreamAnalysisReporter::finished(const std::filesystem::path& /*file*/, const std::string_view output)
{
    writeToolOutput(*_out, output, _keepColor);
}

std::string renderCheckFinished(const Compile::BuildProfile profile)
{
    return alignedVerb("Finished") + ' ' + std::string{ Compile::profileName(profile) } + " check\n";
}

std::string renderAnalysisFailures(const Tool::ExternalTool& tool, const std::vector<Check::FailedAnalysis>& failures)
{
    std::string text;
    bool notRunNamed = false;
    bool failedNamed = false;
    for (const Check::FailedAnalysis& failure : failures) {
        if (const auto* failed = std::get_if<Tool::ToolFailed>(&failure.failure)) {
            text += renderFailedFile(tool, failure.file, *failed);
            failedNamed = true;
        } else if (! notRunNamed) {
            text += renderToolFailure(tool, {}, failure.failure);
            notRunNamed = true;
        }
    }
    if (failedNamed) {
        text += FixErrorsHint;
    }
    return text;
}

}  // namespace scrap::Command
