#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap toolchain uninstall".
 *
 * Takes away the one installed toolchain a specifier names, and says what
 * became of the global default and of the toolchain builds here use when
 * either led to it.
 */
class ToolchainUninstallCommandHandler : public CommandHandler {
public:
    /**
     * @brief Uninstall the toolchain the specifier names.
     *
     * Nothing is taken away when the specifier names the system's
     * compiler, nothing installed, or more than one installed version, or
     * when the global default cannot be read.
     *
     * @param ctx Invocation context.
     * @return 0 when it is taken away, 1 when nothing is, or the global
     *         default cannot be unselected after it.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
