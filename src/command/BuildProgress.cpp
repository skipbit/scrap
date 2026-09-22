#include "command/BuildProgress.h"

#include "build/BuildStep.h"
#include "command/PrintableText.h"

#include <algorithm>
#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace scrap::Command {

namespace {

/// Width the word for what is being done is written in, so the lines line up.
constexpr std::size_t VerbWidth = 12;

/// Starts every sequence a terminal acts on.
constexpr char Escape = '\x1b';

/// Starts every sequence that colours what follows it.
constexpr std::string_view ColorSequenceStart = "\x1b[";

/**
 * Where the colour sequence starting at @p index ends, or @p index when the
 * bytes there start something else.
 *
 * A compiler colours its diagnostics with the sequences that set an
 * attribute, and gcc clears the rest of the line after each of them. Every
 * other sequence is one this has no reason to pass on.
 */
auto colorSequenceEnd(std::string_view text, std::size_t index) -> std::size_t
{
    if (! text.substr(index).starts_with(ColorSequenceStart)) {
        return index;
    }
    std::size_t end = (index + ColorSequenceStart.size());
    while ((end < text.size()) && (text[end] >= '0') && (text[end] <= '?')) {
        ++end;
    }
    if ((end < text.size()) && ((text[end] == 'm') || (text[end] == 'K'))) {
        return (end + 1);
    }
    return index;
}

/**
 * The word for what @p step does.
 */
auto verbFor(const Build::BuildStep& step) -> std::string_view
{
    return step.kind == Build::StepKind::Compile ? "Compiling" : "Linking";
}

/**
 * @p verb written in the width the lines line up in. A verb longer than the
 * width is written as it is, so the lines lose their alignment rather than
 * the count wrapping.
 */
auto alignedVerb(std::string_view verb) -> std::string
{
    std::string text(VerbWidth - std::min(VerbWidth, verb.size()), ' ');
    text += verb;
    return text;
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
    if (output.empty()) {
        return;
    }
    const std::string text = printableOutput(output, _keepColor);
    *_out << text;
    if (! text.ends_with('\n')) {
        *_out << '\n';
    }
}

auto renderBuildFinished() -> std::string
{
    return alignedVerb("Finished") + " debug build\n";
}

auto printableOutput(const std::string_view text, const bool keepColor) -> std::string
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size();) {
        if (text[index] != Escape) {
            result += text[index];
            ++index;
            continue;
        }

        const std::size_t end = colorSequenceEnd(text, index);
        if (end == index) {
            // Anything else the terminal would act on is written as text, as
            // every other string scrap prints is.
            result += "\\x1B";
            ++index;
            continue;
        }
        if (keepColor) {
            result.append(text, index, end - index);
        }
        index = end;
    }
    return result;
}

auto standardErrorIsTerminal() -> bool
{
    return (::isatty(STDERR_FILENO) == 1);
}

}  // namespace scrap::Command
