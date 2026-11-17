#include "command/ToolchainUninstallText.h"

#include "command/NameAfterUninstall.h"
#include "command/PrintableText.h"
#include "command/ToolchainNaming.h"
#include "install/ToolchainRemoval.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * What names a toolchain for builds here, as the lines about it say it.
 */
std::string_view namer(const ChoiceSource source)
{
    switch (source) {
    case ChoiceSource::Variable:
    case ChoiceSource::VariableOverProject:
        return "SCRAP_TOOLCHAIN";
    case ChoiceSource::Project:
        return "scrap.toml";
    case ChoiceSource::GlobalDefault:
        return "the global default";
    }
    // Every source is answered above, so a source added without a word here
    // fails the build rather than being described as one of the others.
    std::unreachable();
}

}  // anonymous namespace

std::string renderUninstalled(const std::string_view name, const std::filesystem::path& directory)
{
    return "Uninstalled '" + printableText(name) + "' from '" + printablePath(directory) + "'\n";
}

std::string renderGlobalDefaultAfterUninstall(const NamedToolchain& named, const NameAfterUninstall& after)
{
    const std::string shown = printableText(named.text);
    if (const auto* elsewhere = std::get_if<LeadsElsewhere>(&after)) {
        return "The global default '" + shown + "' now leads to '" + printableText(elsewhere->name) + "'\n";
    }
    return "Unselected '" + shown + "'; no toolchain is selected globally\n";
}

std::string renderUsedHereAfterUninstall(const NamedToolchain& named, const NameAfterUninstall& after)
{
    const std::string shown = printableText(named.text);
    const std::string names = std::string{ namer(named.source) } + " names '" + shown + "'";
    if (const auto* elsewhere = std::get_if<LeadsElsewhere>(&after)) {
        return names + ", which now leads to '" + printableText(elsewhere->name) + "'\n";
    }
    return "warning: " + names + ", which is no longer installed; builds here stop until it is installed again\n"
         + "hint: run 'scrap toolchain install " + shown + "'\n";
}

std::string renderCannotUninstallSystem()
{
    return "error: 'system' cannot be uninstalled; it is the system's compiler, which scrap does not install\n";
}

std::string renderNothingToUninstall(const std::string_view specifier)
{
    return "error: toolchain '" + printableTypedEcho(specifier) + "' is not installed\n"
         + "hint: run 'scrap toolchain list' to see what is installed\n";
}

std::string renderNamesMoreThanOneToUninstall(const std::string_view specifier, const std::vector<std::string>& names)
{
    std::string listed;
    for (const std::string& name : names) {
        listed += (listed.empty() ? "" : ", ") + printableText(name);
    }
    const std::string example = names.empty() ? std::string{} : printableText(names.back());
    return "error: '" + printableTypedEcho(specifier) + "' names more than one installed toolchain: " + listed + "\n"
         + "hint: name one with its full version, as in 'scrap toolchain uninstall " + example + "'\n";
}

std::string renderNotRemoved(const Install::ToolchainNotRemoved& failure)
{
    return "error: cannot remove '" + printablePath(failure.path) + "': " + printableText(failure.code.message()) + "\n";
}

}  // namespace scrap::Command
