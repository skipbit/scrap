#pragma once

#include "command/ScriptsReader.h"

namespace scrap::Command {

/**
 * @brief Stub ScriptsReader that always returns an empty script list.
 *
 * Used as a placeholder until the configuration module provides
 * a real implementation that reads scrap.toml [scripts].
 */
class StubScriptsReader : public ScriptsReader {
public:
    /**
     * @brief Always return an empty script list.
     *
     * @param projectRoot Project root path (unused).
     * @return Empty ScriptDef vector.
     */
    [[nodiscard]] auto
    read(const std::filesystem::path& projectRoot) -> std::expected<std::vector<ScriptDef>, std::string> override;
};

}  // namespace scrap::Command
