#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap clean [<path>]".
 *
 * Loads the project that the path, or the working directory when no path is
 * given, belongs to, and removes its build directory with everything in it.
 * A project that was never built has nothing to remove, which is not a
 * failure. When there is no such project, or the directory cannot be
 * removed, reports the cause and the next step on standard error.
 */
class CleanCommandHandler : public CommandHandler {
public:
    /**
     * @brief Load the project and remove its build directory.
     *
     * @param ctx Invocation context. Its first positional is the optional path.
     * @return 0 on success, 1 when it reports a failure.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
