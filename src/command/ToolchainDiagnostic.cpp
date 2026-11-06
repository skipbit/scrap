#include "command/ToolchainDiagnostic.h"

#include "command/PermissionHint.h"
#include "command/PrintableText.h"
#include "toolchain/GlobalConfig.h"
#include "toolchain/GlobalConfigFile.h"
#include "toolchain/InstalledToolchain.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace scrap::Command {

namespace {

constexpr std::string_view SpecifierShape = "<family>[@<version>], such as 'gcc@15'";

/**
 * The report that @p text is no specifier, for @p reason, closed by @p ending
 * after the shape of one.
 */
std::string notASpecifier(std::string_view text, std::string_view reason, std::string_view ending)
{
    std::string rendered = "error: '" + printableTypedEcho(text) + "' is not a toolchain specifier: ";
    rendered += reason;
    rendered += "\nhint: write it as ";
    rendered += SpecifierShape;
    rendered += ending;
    return rendered;
}

/**
 * A fault in the text of @p file, pointed at as a compiler points at one.
 */
std::string renderFault(const std::filesystem::path& file, const Toolchain::GlobalConfigFault& fault)
{
    std::string text = printablePath(file) + ":" + std::to_string(fault.line) + ":" + std::to_string(fault.column) + ": error: ";
    if (! fault.key.empty()) {
        text += fault.key + ": ";
    }
    text += printableText(fault.message);
    text += "\nhint: fix the file, or remove it to clear the settings it holds\n";
    return text;
}

std::string render(const Toolchain::CannotReadConfig& failure)
{
    return "error: cannot read '" + printablePath(failure.path) + "'\n" + std::string{ PermissionHint };
}

std::string render(const Toolchain::CannotCreateDirectory& failure)
{
    return "error: cannot create '" + printablePath(failure.path) + "': " + failure.code.message() + "\n" + std::string{ PermissionHint };
}

std::string render(const Toolchain::CannotWriteConfig& failure)
{
    return "error: cannot write '" + printablePath(failure.path) + "': " + failure.code.message() + "\n" + std::string{ PermissionHint };
}

std::string render(std::string_view, const ToolchainNamedIn namedIn, const Toolchain::NoToolchainForHost&)
{
    return renderNoToolchainForHost(namedIn);
}

std::string render(const std::string_view specifier, ToolchainNamedIn, const Toolchain::UnusableInstalled& fault)
{
    return renderUnusableInstalled(specifier, fault.reason);
}

std::string render(const std::string_view specifier, ToolchainNamedIn, const Toolchain::NotInstalled&)
{
    return renderNotInstalled(specifier);
}

std::string render(std::string_view, ToolchainNamedIn, const Toolchain::CannotListToolchains& fault)
{
    return "error: cannot list '" + printablePath(fault.directory) + "': " + fault.code.message() + "\n" + std::string{ PermissionHint };
}

}  // anonymous namespace

std::string renderNotASpecifier(std::string_view text, std::string_view reason)
{
    return notASpecifier(text, reason, ", or as system\n");
}

std::string renderNotInstallable(std::string_view text, std::string_view reason)
{
    return notASpecifier(text, reason, "\n");
}

std::string renderToolchainVariableNotASpecifier(std::string_view value, std::string_view reason)
{
    std::string rendered = "error: SCRAP_TOOLCHAIN holds '" + printableTypedEcho(value) + "', which is not a toolchain specifier: ";
    rendered += reason;
    rendered += "\nhint: set it to ";
    rendered += SpecifierShape;
    rendered += ", or to system, or unset it\n";
    return rendered;
}

std::string renderNotInstalled(std::string_view specifier)
{
    const std::string shown = printableText(specifier);
    std::string rendered = "error: toolchain '" + shown + "' is not installed\nhint: run 'scrap toolchain install ";
    rendered += shown;
    rendered += "'\n";
    return rendered;
}

std::string renderUnusableInstalled(std::string_view specifier, std::string_view reason)
{
    std::string rendered = "error: cannot use the installed toolchain: " + printableText(reason);
    rendered += "\nhint: run 'scrap toolchain install --force ";
    rendered += printableText(specifier);
    rendered += "'\n";
    return rendered;
}

std::string renderNoToolchainForHost(const ToolchainNamedIn namedIn)
{
    const std::string_view error = "error: no toolchain is built for this host\n";
    switch (namedIn) {
    case ToolchainNamedIn::CommandLine:
    case ToolchainNamedIn::GlobalDefault:
        return std::string{ error } + "hint: run 'scrap toolchain select system' to use the compiler found on the system\n";
    case ToolchainNamedIn::Variable:
        return std::string{ error } + "hint: set SCRAP_TOOLCHAIN to system, or unset it, to use the compiler found on the system\n";
    case ToolchainNamedIn::Project:
        return std::string{ error } + "hint: set name under [toolchain] in scrap.toml to \"system\" to use the compiler found on the system\n";
    }
    // Every place is answered above, so a place added without a hint here
    // fails the build rather than being sent to another place's hint.
    std::unreachable();
}

std::string renderInstalledToolchainFault(const std::string_view specifier, const ToolchainNamedIn namedIn, const Toolchain::InstalledToolchainFault& fault)
{
    return std::visit([specifier, namedIn](const auto& alternative) -> std::string {
        return render(specifier, namedIn, alternative);
    }, fault);
}

std::string renderNoScrapHome()
{
    return "error: cannot tell where to keep toolchains: SCRAP_HOME and HOME are both unset\n"
           "hint: set SCRAP_HOME to the directory to keep them in, and run the command again\n";
}

std::string renderGlobalConfigFileError(const std::filesystem::path& file, const Toolchain::GlobalConfigFileError& error)
{
    return std::visit(
        [&file](const auto& alternative) -> std::string {
        if constexpr (std::is_same_v<std::decay_t<decltype(alternative)>, Toolchain::GlobalConfigFault>) {
            return renderFault(file, alternative);
        } else {
            return render(alternative);
        }
    },
        error);
}

std::string renderNoCompilerFound()
{
    return "error: no C++ compiler found\n"
           "hint: install a C++ compiler, or set CXX to the one to use\n";
}

/**
 * The value is echoed through printableEcho(), since the environment can hold any
 * byte and the answer is read in a terminal.
 */
std::string renderUnusableCompilerRequest(const std::string_view requested)
{
    std::string text = "error: CXX names '";
    text += printableEcho(requested);
    text += "', which cannot be run\n";
    text += "hint: set CXX to the path of a compiler, or unset it to search for one\n";
    return text;
}

std::string renderNoCompilerWithInstalled(std::string_view example)
{
    std::string rendered = "error: no C++ compiler found\nhint: select an installed toolchain with 'scrap toolchain select <toolchain>', such as '";
    rendered += printableText(example);
    rendered += "'\n";
    return rendered;
}

}  // namespace scrap::Command
