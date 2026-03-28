#pragma once

#include "command/CommandResolver.h"
#include "command/ScriptsReader.h"

#include <memory>

namespace scrap::Command {

/**
 * @brief Resolver for project-scoped commands defined in scrap.toml.
 *
 * Delegates to a ScriptsReader to parse the [scripts] section.
 * Returns an empty list if the project root has no configuration
 * or if reading fails.
 */
class ProjectCommandResolver : public CommandResolver {
public:
    /**
     * @brief Construct with a ScriptsReader for parsing project scripts.
     *
     * @param scriptsReader Reader that parses scrap.toml [scripts] section.
     */
    explicit ProjectCommandResolver(std::unique_ptr<ScriptsReader> scriptsReader) noexcept;

    /**
     * @brief Read project scripts and convert to CommandEntry list.
     *
     * Returns an empty vector if the project has no configuration
     * or if reading fails (graceful degradation).
     *
     * @param env Runtime environment containing the project root path.
     * @return CommandEntry list for project-scoped script commands.
     */
    auto resolve(const RuntimeEnvironment& env) -> std::vector<CommandEntry> override;

private:
    std::unique_ptr<ScriptsReader> scriptsReader_;
};

}  // namespace scrap::Command
