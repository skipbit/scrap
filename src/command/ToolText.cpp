#include "command/ToolText.h"

#include "command/PrintableText.h"
#include "tool/ExternalTool.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <variant>

namespace scrap::Command {

namespace {

/// The line saying that @p program could not be started.
std::string render(std::string_view /*name*/, const std::filesystem::path& program, const std::string& /*heading*/, const Tool::ToolNotRun& notRun)
{
    return "error: cannot run '" + printablePath(program) + "': " + notRun.code.message() + '\n';
}

/**
 * @p heading, the signal that stopped @p name when one did, or its exit
 * status when it wrote nothing, and then what it wrote.
 */
std::string render(const std::string_view name, const std::filesystem::path& /*program*/, const std::string& heading, const Tool::ToolFailed& failed)
{
    std::string text = heading;
    if (failed.signal.has_value()) {
        text += ": ";
        text += name;
        text += " was stopped by signal ";
        text += std::to_string(*failed.signal);
    } else if (failed.output.empty() && failed.exitCode.has_value()) {
        // With nothing written, the status is all there is to say why.
        text += ": ";
        text += name;
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

}  // anonymous namespace

std::string renderToolInUse(const std::string_view name, const Tool::ExternalTool& tool)
{
    return "Using " + std::string{ name } + " '" + printablePath(tool.program) + "' (version " + printableText(tool.version) + ")\n";
}

std::string renderToolNotFound(const std::string_view name)
{
    std::string text = "error: ";
    text += name;
    text += " not found in PATH\nhint: install ";
    text += name;
    text += " and make sure it is on PATH\n";
    return text;
}

std::string renderVersionFailure(const std::string_view name, const std::filesystem::path& program, const Tool::ToolFailure& failure)
{
    return renderToolFailure(name, program, "error: cannot learn the version of '" + printablePath(program) + "'", failure);
}

std::string renderToolFailure(const std::string_view name,
                              const std::filesystem::path& program,
                              const std::string& heading,
                              const Tool::ToolFailure& failure)
{
    return std::visit([name, &program, &heading](const auto& alternative) -> std::string {
        return render(name, program, heading, alternative);
    }, failure);
}

}  // namespace scrap::Command
