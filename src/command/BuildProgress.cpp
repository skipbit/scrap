#include "command/BuildProgress.h"

#include "build/BuildStep.h"
#include "command/ProjectDiagnostic.h"

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace scrap::Command {

namespace {

/// Width the word for what is being done is written in, so the lines line up.
constexpr std::size_t VerbWidth = 12;

/// Starts every sequence that colours what follows it.
constexpr std::string_view ColorSequenceStart = "\x1b[";

/**
 * The word for what @p step does.
 */
auto verbFor(const Build::BuildStep& step) -> std::string_view
{
    return step.kind == Build::StepKind::Compile ? "Compiling" : "Linking";
}

/**
 * @p verb written in the width the lines line up in.
 */
auto alignedVerb(std::string_view verb) -> std::string
{
    std::string text(VerbWidth - verb.size(), ' ');
    text += verb;
    return text;
}

}  // anonymous namespace

StreamBuildReporter::StreamBuildReporter(std::ostream& out, const bool keepColor)
    : out_(&out), keepColor_(keepColor)
{
}

void StreamBuildReporter::started(const Build::BuildStep& step)
{
    *out_ << alignedVerb(verbFor(step)) << ' ' << printableName(step.target) << " (" << printablePath(step.subject)
          << ")\n";
}

void StreamBuildReporter::finished(const Build::BuildStep& /*step*/, const std::string_view output)
{
    if (output.empty()) {
        return;
    }
    const std::string text = keepColor_ ? std::string{output} : withoutColor(output);
    *out_ << text;
    if (! text.ends_with('\n')) {
        *out_ << '\n';
    }
}

auto renderBuildFinished() -> std::string
{
    return alignedVerb("Finished") + " debug build\n";
}

auto withoutColor(const std::string_view text) -> std::string
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size();) {
        if (! text.substr(index).starts_with(ColorSequenceStart)) {
            result += text[index];
            ++index;
            continue;
        }
        std::size_t end = index + ColorSequenceStart.size();
        while (end < text.size() && text[end] >= '0' && text[end] <= '?') {
            ++end;
        }
        if (end < text.size() && (text[end] == 'm' || text[end] == 'K')) {
            index = end + 1;  // The sequence colours; it is not part of what was written.
            continue;
        }
        result += text[index];
        ++index;
    }
    return result;
}

auto standardErrorIsTerminal() -> bool
{
    return ::isatty(STDERR_FILENO) == 1;
}

}  // namespace scrap::Command
