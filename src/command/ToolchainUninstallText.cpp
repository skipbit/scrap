#include "command/ToolchainUninstallText.h"

#include "command/NameAfterUninstall.h"
#include "command/PrintableText.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainNaming.h"
#include "install/ToolchainRemoval.h"

#include <cstddef>
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
    return renderUnselected(named.text);
}

std::string renderUsedHereAfterUninstall(const NamedToolchain& named, const NameAfterUninstall& after)
{
    const std::string shown = printableText(named.text);
    std::string names{ namer(named.source) };
    names += " names '" + shown + "'";
    if (const auto* elsewhere = std::get_if<LeadsElsewhere>(&after)) {
        return names + ", which now leads to '" + printableText(elsewhere->name) + "'\n";
    }
    std::string rendered = "warning: " + names;
    rendered += ", which is no longer installed; builds here stop until it is installed again\nhint: run 'scrap toolchain install ";
    rendered += shown;
    rendered += "'\n";
    return rendered;
}

std::string renderCannotUninstallSystem()
{
    return "error: 'system' cannot be uninstalled; it is the system's compiler, which scrap does not install\n";
}

std::string renderNothingToUninstall(const std::string_view specifier)
{
    std::string rendered = "error: toolchain '" + printableTypedEcho(specifier);
    rendered += "' is not installed\nhint: run 'scrap toolchain list' to see what is installed\n";
    return rendered;
}

std::string renderNamesMoreThanOneToUninstall(const std::string_view specifier, const std::vector<std::string>& names)
{
    std::string rendered = "error: '" + printableTypedEcho(specifier);
    rendered += "' names more than one installed toolchain: ";
    for (std::size_t i = 0; i < names.size(); ++i) {
        rendered += (i == 0) ? "" : ", ";
        rendered += printableText(names[i]);
    }
    rendered += "\nhint: name one with its full version, as in 'scrap toolchain uninstall ";
    rendered += names.empty() ? std::string{} : printableText(names.back());
    rendered += "'\n";
    return rendered;
}

std::string renderLeftBehind(const std::filesystem::path& path)
{
    return "warning: could not delete everything in '" + printablePath(path) + "'; it can be removed by hand\n";
}

std::string renderNotRemoved(const Install::ToolchainNotRemoved& failure)
{
    std::string rendered = "error: cannot remove '" + printablePath(failure.path) + "': ";
    if (failure.workDirectory.has_value()) {
        rendered += "cannot make '" + printablePath(*failure.workDirectory) + "': ";
    }
    rendered += printableText(failure.code.message());
    rendered += "\n";
    return rendered;
}

}  // namespace scrap::Command
