#include "command/HelpRequest.h"

#include "command/CommandCatalog.h"
#include "command/CommandSpec.h"
#include "command/HelpRenderer.h"
#include "command/PrintableText.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace scrap::Command {

namespace {

constexpr int UsageErrorExitCode = 2;

/**
 * @p words separated by spaces, as the user types them.
 */
std::string joined(std::span<const std::string> words)
{
    std::string text;
    for (std::size_t index = 0; index < words.size(); ++index) {
        if (index > 0) {
            text += ' ';
        }
        text += words[index];
    }
    return text;
}

/**
 * Report that @p target names no command, pointing at the help of the
 * deepest command found along it, the first @p depth words, which is
 * @p found, or at the global help when no word names a command.
 */
int reportUnknown(std::span<const std::string> target, std::size_t depth, const CommandSpec* found)
{
    // The name is quoted so that an empty one still reads as a name.
    std::cerr << "error: unknown command '" << printableText(joined(target)) << "'\n";
    if (found == nullptr) {
        std::cerr << "hint: run 'scrap --help' to list the commands\n";
    } else if (found->subcommands.empty()) {
        std::cerr << "hint: run 'scrap " << printableText(joined(target.first(depth))) << " --help' for its usage\n";
    } else {
        std::cerr << "hint: run 'scrap " << printableText(joined(target.first(depth))) << " --help' to list its commands\n";
    }
    return UsageErrorExitCode;
}

}  // namespace

int showHelp(const HelpRenderer& renderer, const CommandCatalog& catalog, std::span<const std::string> target)
{
    const std::vector<CommandSpec> specs = catalog.specs();
    const CommandSpec* found = nullptr;
    for (std::size_t depth = 0; depth < target.size(); ++depth) {
        const std::vector<CommandSpec>& candidates = (found == nullptr) ? specs : found->subcommands;
        const auto match = std::ranges::find(candidates, target[depth], &CommandSpec::name);
        if (match == candidates.end()) {
            return reportUnknown(target, depth, found);
        }
        found = &*match;
    }
    // Only a target without words leaves nothing found.
    if (found == nullptr) {
        std::cout << renderer.renderGlobal(catalog.helpEntries());
    } else {
        std::cout << renderer.renderCommand(joined(target), *found);
    }
    return 0;
}

}  // namespace scrap::Command
