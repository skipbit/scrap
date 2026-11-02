#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap toolchain select <specifier>".
 *
 * Keeps the toolchain the specifier names as the global default. A
 * specifier is matched against the installed toolchains alone and kept as
 * the newest one it names, so a later install does not change what is
 * selected. The word system selects the compiler found on the system.
 */
class ToolchainSelectCommandHandler : public CommandHandler {
public:
    /**
     * @brief Select the toolchain and keep it in config.toml.
     *
     * @param ctx Invocation context. Its first positional is the specifier.
     * @return 0 on success, 1 when it reports a failure.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
