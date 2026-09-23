#include "command/HelpRequest.h"

#include "command/CommandCatalog.h"
#include "command/HelpRenderer.h"
#include "command/PrintableText.h"

#include <iostream>
#include <optional>
#include <string_view>

namespace scrap::Command {

namespace {

constexpr int UsageErrorExitCode = 2;

}  // namespace

int showHelp(const HelpRenderer& renderer, const CommandCatalog& catalog, std::optional<std::string_view> target)
{
    if (! target.has_value()) {
        std::cout << renderer.renderGlobal(catalog.helpEntries());
        return 0;
    }
    for (const auto& spec : catalog.specs()) {
        if (spec.name == *target) {
            std::cout << renderer.renderCommand(spec);
            return 0;
        }
    }
    // The name is quoted so that an empty one still reads as a name.
    std::cerr << "error: unknown command '" << printableText(*target) << "'\n"
              << "hint: run 'scrap --help' to list the commands\n";
    return UsageErrorExitCode;
}

}  // namespace scrap::Command
