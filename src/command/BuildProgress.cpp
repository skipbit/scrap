#include "command/BuildProgress.h"

#include "build/BuildStep.h"
#include "command/PrintableText.h"
#include "command/ProgressLine.h"
#include "compile/BuildProfile.h"

#include <cstddef>
#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>

namespace scrap::Command {

namespace {

/// Starts every sequence a terminal acts on.
constexpr char Escape = '\x1b';

/// Starts every sequence that colours what follows it.
constexpr std::string_view ColorSequenceStart = "\x1b[";

/**
 * Where the colour sequence starting at @p index ends, or @p index when the
 * bytes there start something else.
 *
 * A compiler colours its diagnostics with the sequences that set an
 * attribute, whose parameters are digits and ';', and gcc clears the rest of
 * the line after each of them. Every other sequence, one that erases more or
 * takes another parameter among them, is one this has no reason to pass on.
 */
std::size_t colorSequenceEnd(std::string_view text, std::size_t index)
{
    if (! text.substr(index).starts_with(ColorSequenceStart)) {
        return index;
    }
    const std::size_t parametersStart = (index + ColorSequenceStart.size());
    std::size_t end = parametersStart;
    while ((end < text.size()) && (((text[end] >= '0') && (text[end] <= '9')) || (text[end] == ';'))) {
        ++end;
    }
    if (end == text.size()) {
        return index;
    }
    const std::string_view parameters = text.substr(parametersStart, end - parametersStart);
    if ((text[end] == 'm') || ((text[end] == 'K') && (parameters.empty() || (parameters == "0")))) {
        return (end + 1);
    }
    return index;
}

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
    if (output.empty()) {
        return;
    }
    const std::string text = printableOutput(output, _keepColor);
    *_out << text;
    if (! text.ends_with('\n')) {
        *_out << '\n';
    }
}

std::string renderBuildFinished(const Compile::BuildProfile profile)
{
    return alignedVerb("Finished") + ' ' + std::string{ Compile::profileName(profile) } + " build\n";
}

std::string renderRunning(const std::string_view target, const std::filesystem::path& executable)
{
    return alignedVerb("Running") + ' ' + printableName(target) + " (" + printablePath(executable) + ")\n";
}

std::string printableOutput(const std::string_view text, const bool keepColor)
{
    std::string result;
    result.reserve(text.size());
    std::size_t start = 0;
    std::size_t index = 0;
    while (index < text.size()) {
        const std::size_t end = (text[index] == Escape) ? colorSequenceEnd(text, index) : index;
        if (end == index) {
            ++index;
            continue;
        }
        // What lies between the colour sequences, an escape that starts
        // anything else among it, is written as every other string scrap
        // prints is.
        result += printableLines(text.substr(start, index - start));
        if (keepColor) {
            result.append(text, index, end - index);
        }
        index = end;
        start = end;
    }
    result += printableLines(text.substr(start));
    return result;
}

bool standardErrorIsTerminal()
{
    return (::isatty(STDERR_FILENO) == 1);
}

}  // namespace scrap::Command
