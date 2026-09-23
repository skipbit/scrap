#include "command/StubScriptsReader.h"

#include "command/ScriptsReader.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <vector>

namespace scrap::Command {

/**
 * Always return an empty script list (stub implementation).
 */
// NOLINTNEXTLINE(readability-convert-member-functions-to-static) - virtual override
std::expected<std::vector<ScriptDef>, std::string> StubScriptsReader::read([[maybe_unused]] const std::filesystem::path& projectRoot)
{
    return std::vector<ScriptDef>{};
}

}  // namespace scrap::Command
