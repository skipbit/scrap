#include "command/FormatText.h"

#include "command/PrintableText.h"
#include "format/ClangFormat.h"

#include <filesystem>
#include <string>
#include <variant>

namespace scrap::Command {

namespace {

/// The line saying that @p program could not be started.
std::string cannotRun(const std::filesystem::path& program, const Format::ToolNotRun& notRun)
{
    return "error: cannot run '" + printablePath(program) + "': " + notRun.code.message() + '\n';
}

/**
 * @p first, the signal that stopped the program when one did, and then what
 * the program wrote.
 */
std::string withOutput(std::string first, const Format::ToolFailed& failed)
{
    if (failed.signal.has_value()) {
        first += ": clang-format was stopped by signal ";
        first += std::to_string(*failed.signal);
    }
    first += '\n';
    first += printableLines(failed.output);
    if ((! failed.output.empty()) && (! first.ends_with('\n'))) {
        first += '\n';
    }
    return first;
}

}  // anonymous namespace

std::string renderClangFormatInUse(const Format::ClangFormat& tool)
{
    return "Using clang-format '" + printablePath(tool.program) + "' (version " + printableText(tool.version) + ")\n";
}

std::string renderClangFormatNotFound()
{
    return "error: clang-format not found in PATH\n"
           "hint: install clang-format and make sure it is on PATH\n";
}

std::string renderVersionFailure(const std::filesystem::path& program, const Format::ToolFailure& failure)
{
    if (const auto* const notRun = std::get_if<Format::ToolNotRun>(&failure)) {
        return cannotRun(program, *notRun);
    }
    return withOutput("error: cannot learn the version of '" + printablePath(program) + "'", std::get<Format::ToolFailed>(failure));
}

std::string renderFormatFailure(const Format::ClangFormat& tool, const std::filesystem::path& file, const Format::ToolFailure& failure)
{
    if (const auto* const notRun = std::get_if<Format::ToolNotRun>(&failure)) {
        return cannotRun(tool.program, *notRun);
    }
    return withOutput("error: failed to format '" + printablePath(file) + "'", std::get<Format::ToolFailed>(failure));
}

}  // namespace scrap::Command
