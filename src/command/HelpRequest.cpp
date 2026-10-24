#include "command/HelpRequest.h"

#include "command/CommandCatalog.h"
#include "command/CommandPath.h"
#include "command/HelpRenderer.h"
#include "command/PrintableText.h"

#include <iostream>
#include <span>
#include <string>

namespace scrap::Command {

namespace {

constexpr int UsageErrorExitCode = 2;

/**
 * Report that @p target names no command, pointing at the help of the
 * deepest command @p found along it, or at the global help when no word
 * names a command.
 */
int reportUnknown(std::span<const std::string> target, const CommandCatalog::PathMatch& found)
{
    // The name is quoted so that an empty one still reads as a name.
    std::cerr << "error: unknown command '" << printableText(typedPath(target)) << "'\n";
    if (found.deepest == nullptr) {
        std::cerr << "hint: run 'scrap --help' to list the commands\n";
    } else if (found.deepest->subcommands.empty()) {
        std::cerr << "hint: run 'scrap " << printableText(typedPath(target.first(found.depth))) << " --help' for its usage\n";
    } else {
        std::cerr << "hint: run 'scrap " << printableText(typedPath(target.first(found.depth))) << " --help' to list its commands\n";
    }
    return UsageErrorExitCode;
}

}  // namespace

int showHelp(const HelpRenderer& renderer, const CommandCatalog& catalog, std::span<const std::string> target)
{
    if (target.empty()) {
        std::cout << renderer.renderGlobal(catalog.helpEntries());
        return 0;
    }
    const auto found = catalog.match(target);
    if (found.depth != target.size()) {
        return reportUnknown(target, found);
    }
    std::cout << renderer.renderCommand(typedPath(target), CommandCatalog::specOf(*found.deepest));
    return 0;
}

}  // namespace scrap::Command
