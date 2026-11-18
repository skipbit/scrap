#pragma once

#include "command/CommandHandler.h"

#include <string_view>

namespace scrap::Command {

inline constexpr std::string_view RemoteOption = "remote";
inline constexpr std::string_view AllOption = "all";

/**
 * @brief Handler for "scrap toolchain list".
 *
 * Lists the toolchains to choose from on standard output: the system's
 * compiler when found, then the installed toolchains, marked as the global
 * default and as the one a build in the project here uses. With --remote,
 * lists the toolchains of the saved copy of the index instead, the ones the
 * host can install or, with --all, every one.
 */
class ToolchainListCommandHandler : public CommandHandler {
public:
    /**
     * @brief List the toolchains to choose from.
     *
     * What names the default or the toolchain used here and cannot be read
     * is warned about, and the rest is listed. Installed toolchains that
     * cannot be listed stop it. The index is not fetched: without a saved
     * copy, --remote fails.
     *
     * @param ctx Invocation context.
     * @return 0 when it lists them, 1 when it reports a failure.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
