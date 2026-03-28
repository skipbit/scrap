#include "command/StubScriptsReader.h"

namespace scrap::Command {

auto StubScriptsReader::read([[maybe_unused]] const std::filesystem::path& projectRoot)
    -> std::expected<std::vector<ScriptDef>, std::string>
{
    return std::vector<ScriptDef>{};
}

}  // namespace scrap::Command
