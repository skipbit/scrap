#include "command/ToolchainDiagnostic.h"

#include "command/PrintableText.h"

#include <string>
#include <string_view>

namespace scrap::Command {

namespace {

constexpr std::string_view SpecifierShape = "<family>[@<version>], such as 'gcc@15'";

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

std::string renderNoCompilerWithInstalled(std::string_view example)
{
    std::string rendered = "error: no C++ compiler found\nhint: select an installed toolchain with 'scrap toolchain select <toolchain>', such as '";
    rendered += printableText(example);
    rendered += "'\n";
    return rendered;
}

}  // namespace scrap::Command
