#include "command/FormatText.h"

#include "command/PrintableText.h"
#include "format/ClangFormat.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <variant>

namespace scrap::Command {

namespace {

/// The tool as the messages name it.
constexpr std::string_view ToolName = "clang-format";

/// The line saying that @p program could not be started.
std::string render(const std::filesystem::path& program, std::string_view /*toolName*/, const Format::ToolNotRun& notRun, const std::string& /*heading*/)
{
    return "error: cannot run '" + printablePath(program) + "': " + notRun.code.message() + '\n';
}

/**
 * @p heading, the signal that stopped @p toolName when one did, or its exit
 * status when it wrote nothing, and then what it wrote.
 */
std::string render(const std::filesystem::path& /*program*/, const std::string_view toolName, const Format::ToolFailed& failed, const std::string& heading)
{
    std::string text = heading;
    if (failed.signal.has_value()) {
        text += ": ";
        text += toolName;
        text += " was stopped by signal ";
        text += std::to_string(*failed.signal);
    } else if (failed.output.empty() && failed.exitCode.has_value()) {
        // With nothing written, the status is all there is to say why.
        text += ": ";
        text += toolName;
        text += " exited with status ";
        text += std::to_string(*failed.exitCode);
    }
    text += '\n';
    text += printableLines(failed.output);
    if ((! failed.output.empty()) && (! text.ends_with('\n'))) {
        text += '\n';
    }
    return text;
}

/**
 * What failed to run @p program, named @p toolName, under @p heading.
 */
std::string renderToolFailure(const std::filesystem::path& program, const std::string_view toolName, const std::string& heading, const Format::ToolFailure& failure)
{
    return std::visit([&program, toolName, &heading](const auto& alternative) -> std::string {
        return render(program, toolName, alternative, heading);
    }, failure);
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
    return renderToolFailure(program, ToolName, "error: cannot learn the version of '" + printablePath(program) + "'", failure);
}

std::string renderFormatFailure(const Format::ClangFormat& tool, const std::filesystem::path& file, const Format::ToolFailure& failure)
{
    return renderToolFailure(tool.program, ToolName, "error: failed to format '" + printablePath(file) + "'", failure);
}

}  // namespace scrap::Command
