#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap new <project-name>".
 *
 * Creates the project from the built-in template in the working directory
 * and reports the project's name and location, or the cause of a failure and
 * the next step on standard error.
 */
class NewCommandHandler : public CommandHandler {
public:
    /**
     * @brief Create the project named by the first positional.
     *
     * @param ctx Invocation context. Its first positional is the project name.
     * @return 0 when the project was created, 1 otherwise.
     */
    auto execute(const InvocationContext& ctx) -> int override;
};

}  // namespace scrap::Command
