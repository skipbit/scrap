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
    /**
     * @brief Construct with renderer references for help/version commands.
     *
     * @param helpRenderer    Renderer used by the help command handler.
     * @param versionRenderer Renderer used by the version command handler.
     */
    BuiltinCommandResolver(HelpRenderer& helpRenderer, VersionRenderer& versionRenderer);

    /**
     * @brief Return the fixed set of built-in command entries.
     *
     * The result is constant regardless of the runtime environment.
     *
     * @param env Runtime environment (unused by this resolver).
     * @return CommandEntry trees for all built-in commands.
     */
    auto resolve(const RuntimeEnvironment& env) -> std::vector<CommandEntry> override;

private:
    HelpRenderer& helpRenderer_;
    VersionRenderer& versionRenderer_;
};

}  // namespace scrap::Command
