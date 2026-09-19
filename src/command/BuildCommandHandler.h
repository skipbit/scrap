#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap build [<path>]".
 *
 * Loads the project that the path, or the working directory when no path is
 * given, belongs to, and decides what it builds. When there is no such
 * project, its manifest is wrong, nothing was found to build, or its sources
 * cannot be read, reports the cause and the next step on standard error.
 */
class BuildCommandHandler : public CommandHandler {
public:
    /**
     * @brief Load the project and build it.
     *
     * @param ctx Invocation context. Its first positional is the optional path.
     * @return 0 on success, 1 when the project cannot be loaded or has no
     *         target to build.
     */
    auto execute(const InvocationContext& ctx) -> int override;
};

}  // namespace scrap::Command
