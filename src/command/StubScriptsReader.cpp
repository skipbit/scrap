#include "command/StubScriptsReader.h"

#include "command/ScriptsReader.h"

#include <expected>  // NOLINT(misc-include-cleaner) — provides std::expected return type
#include <filesystem>
#include <string>
#include <vector>

namespace scrap::Command {

/**
 * Always return an empty script list (stub implementation).
 */
// NOLINTNEXTLINE(readability-convert-member-functions-to-static) — virtual override
auto StubScriptsReader::read([[maybe_unused]] const std::filesystem::path& projectRoot)
    -> std::expected<std::vector<ScriptDef>, std::string>
{
    return std::vector<ScriptDef>{};
}

}  // namespace scrap::Command
