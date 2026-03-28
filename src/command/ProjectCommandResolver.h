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
    explicit ProjectCommandResolver(std::unique_ptr<ScriptsReader> scriptsReader) noexcept;

    auto resolve(const RuntimeEnvironment& env) -> std::vector<CommandEntry> override;

private:
    std::unique_ptr<ScriptsReader> scriptsReader_;
};

}  // namespace scrap::Command
