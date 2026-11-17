#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap toolchain info".
 *
 * Tells one toolchain on standard output: the newest installed one a
 * specifier names, as a build uses it, or, when none is installed, the
 * one install would choose as the saved copy of the index holds it, or
 * the system's compiler.
 */
class ToolchainInfoCommandHandler : public CommandHandler {
public:
    /**
     * @brief Tell the toolchain the specifier names.
     *
     * The index is not fetched: a toolchain that is not installed is told
     * from the copy saved when it was last fetched.
     *
     * @param ctx Invocation context.
     * @return 0 when it tells one, 1 when the specifier names nothing
     *         installed nor in the saved index, or another failure is
     *         reported.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
