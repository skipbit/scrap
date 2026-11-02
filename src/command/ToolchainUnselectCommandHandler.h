#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap toolchain unselect".
 *
 * Removes the global default toolchain. With none selected, there is
 * nothing to remove, which is not a failure.
 */
class ToolchainUnselectCommandHandler : public CommandHandler {
public:
    /**
     * @brief Remove the global default from config.toml.
     *
     * @param ctx Invocation context.
     * @return 0 on success, 1 when it reports a failure.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
