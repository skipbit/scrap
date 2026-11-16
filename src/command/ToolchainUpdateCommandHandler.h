#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap toolchain update".
 *
 * Fetches the toolchain index and saves it, and tells what changed for the
 * host since the copy saved before. Installed toolchains and the global
 * default are left as they are.
 */
class ToolchainUpdateCommandHandler : public CommandHandler {
public:
    /**
     * @brief Fetch the index and report what changed.
     *
     * @param ctx Invocation context.
     * @return 0 on success, 1 when the index cannot be fetched or another
     *         failure is reported.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
