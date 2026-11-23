#include "command/BuildProgress.h"

#include "build/BuildStep.h"
#include "command/PrintableText.h"
#include "command/ProgressLine.h"
#include "command/ToolOutput.h"
#include "compile/BuildProfile.h"

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::Command {

namespace {

/**
 * The word for what @p step does.
 */
std::string_view verbFor(const Build::BuildStep& step)
{
    switch (step.kind) {
    case Build::StepKind::Compile:
        return "Compiling";
    case Build::StepKind::Archive:
        return "Archiving";
    case Build::StepKind::Link:
        return "Linking";
    }
    std::unreachable();
}

}  // anonymous namespace

StreamBuildReporter::StreamBuildReporter(std::ostream& out, const bool keepColor)
    : _out(&out)
    , _keepColor(keepColor)
{
}

void StreamBuildReporter::started(const Build::BuildStep& step)
{
    *_out << alignedVerb(verbFor(step)) << ' ' << printableName(step.target) << " (" << printablePath(step.subject) << ")\n";
}

void StreamBuildReporter::finished(const Build::BuildStep& /*step*/, const std::string_view output)
{
    writeToolOutput(*_out, output, _keepColor);
}

std::string renderBuildFinished(const Compile::BuildProfile profile)
{
    return alignedVerb("Finished") + ' ' + std::string{ Compile::profileName(profile) } + " build\n";
}

std::string renderRunning(const std::string_view target, const std::filesystem::path& executable)
{
    return alignedVerb("Running") + ' ' + printableName(target) + " (" + printablePath(executable) + ")\n";
}

}  // namespace scrap::Command
