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
     * @param env Runtime environment; scripts are read from its working directory.
     * @return CommandEntry list for project-scoped script commands.
     */
    std::vector<CommandEntry> resolve(const RuntimeEnvironment& env) override;

private:
    std::unique_ptr<ScriptsReader> _scriptsReader;
};

}  // namespace scrap::Command
