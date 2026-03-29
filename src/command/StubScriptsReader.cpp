#include "command/StubScriptsReader.h"

#include "command/ScriptsReader.h"

#include <filesystem>
#include <vector>

namespace scrap::Command {

/**
 * Always return an empty script list (stub implementation).
 */
auto StubScriptsReader::read([[maybe_unused]] const std::filesystem::path& projectRoot)
    -> std::expected<std::vector<ScriptDef>, std::string>
{
    return std::vector<ScriptDef>{};
}

}  // namespace scrap::Command
