#include "command/ProjectCommandResolver.h"

#include "command/CommandEntry.h"
#include "command/CommandSource.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScriptsReader.h"

#include <memory>
#include <utility>
#include <vector>

namespace scrap::Command {

/**
 * Construct with a ScriptsReader for parsing project script definitions.
 */
ProjectCommandResolver::ProjectCommandResolver(std::unique_ptr<ScriptsReader> scriptsReader) noexcept
    : scriptsReader_(std::move(scriptsReader))
{
}

/**
 * Read project scripts and convert them to CommandEntry list.
 * Returns empty on any read error (graceful degradation).
 */
auto ProjectCommandResolver::resolve(const RuntimeEnvironment& env) -> std::vector<CommandEntry>
{
    auto result = scriptsReader_->read(env.projectRoot);
    if (! result.has_value()) {
        return {};
    }

    std::vector<CommandEntry> entries;
    for (auto& scriptDef : *result) {
        CommandEntry entry;
        entry.spec.name = std::move(scriptDef.name);
        entry.spec.description = std::move(scriptDef.description);
        entry.spec.category = "Project Scripts";
        entry.source = CommandSource::Project;
        auto command = std::move(scriptDef.command);
        entry.createHandler = [command](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
            // Script execution will be implemented later.
            return nullptr;
        };
        entries.push_back(std::move(entry));
    }

    return entries;
}

}  // namespace scrap::Command
