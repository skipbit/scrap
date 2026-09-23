#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap build [<path>]".
 *
 * Loads the project that the path, or the working directory when no path is
 * given, belongs to, decides what it builds, and writes the command for each
 * source to compile_commands.json in the build directory. When there is no
 * such project, its manifest is wrong, nothing was found to build, its
 * sources cannot be read, or the database cannot be written, reports the
 * cause and the next step on standard error.
 */
class BuildCommandHandler : public CommandHandler {
public:
    /**
     * @brief Load the project and build it.
     *
     * @param ctx Invocation context. Its first positional is the optional path.
     * @return 0 on success, 1 when it reports a failure.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
