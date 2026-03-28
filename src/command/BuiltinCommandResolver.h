#pragma once

#include "command/CommandResolver.h"
#include "command/HelpRenderer.h"
#include "command/VersionRenderer.h"

namespace scrap::Command {

/**
 * @brief Resolver for compile-time built-in commands.
 *
 * Returns a fixed set of CommandEntry trees for help, version,
 * and placeholder entries for existing domain commands (project,
 * toolchain, template).  HelpRenderer and VersionRenderer are
 * injected via constructor and captured by reference in the
 * HandlerFactory lambdas.
 */
class BuiltinCommandResolver : public CommandResolver {
public:
    BuiltinCommandResolver(HelpRenderer& helpRenderer, VersionRenderer& versionRenderer);

    auto resolve(const RuntimeEnvironment& env) -> std::vector<CommandEntry> override;

private:
    HelpRenderer& helpRenderer_;
    VersionRenderer& versionRenderer_;
};

}  // namespace scrap::Command
