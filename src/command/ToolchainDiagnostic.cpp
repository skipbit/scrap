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
#include <variant>

namespace scrap::Command {

namespace {

constexpr std::string_view SpecifierShape = "<family>[@<version>], such as 'gcc@15'";

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

std::string render(std::string_view, const Toolchain::NoToolchainForHost&)
{
    return renderNoToolchainForHost();
}

std::string render(const std::string_view specifier, const Toolchain::UnusableInstalled& fault)
{
    return renderUnusableInstalled(specifier, fault.reason);
}

std::string render(const std::string_view specifier, const Toolchain::NotInstalled&)
{
    return renderNotInstalled(specifier);
}

}  // anonymous namespace

std::string renderNotASpecifier(std::string_view text, std::string_view reason)
{
    std::string rendered = "error: '" + printableText(text) + "' is not a toolchain specifier: ";
    rendered += reason;
    rendered += "\nhint: write it as ";
    rendered += SpecifierShape;
    rendered += ", or as system\n";
    return rendered;
}

std::string renderToolchainVariableNotASpecifier(std::string_view value, std::string_view reason)
{
    std::string rendered = "error: SCRAP_TOOLCHAIN holds '" + printableText(value) + "', which is not a toolchain specifier: ";
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

std::string renderNoToolchainForHost()
{
    return "error: no toolchain is built for this host\n"
           "hint: run 'scrap toolchain select system' to use the compiler found on the system\n";
}

std::string renderInstalledToolchainFault(const std::string_view specifier, const Toolchain::InstalledToolchainFault& fault)
{
    return std::visit([specifier](const auto& alternative) -> std::string {
        return render(specifier, alternative);
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

std::string renderNoCompilerWithInstalled(std::string_view example)
{
    std::string rendered = "error: no C++ compiler found\nhint: select an installed toolchain with 'scrap toolchain select <toolchain>', such as '";
    rendered += printableText(example);
    rendered += "'\n";
    return rendered;
}

}  // namespace scrap::Command
