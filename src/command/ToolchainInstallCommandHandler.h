#pragma once

#include "command/CommandHandler.h"

#include <string_view>

namespace scrap::Command {

/// The option that replaces what is already installed.
inline constexpr std::string_view ForceOption = "force";

/**
 * @brief Handler for "scrap toolchain install [--force] <specifier>".
 *
 * Fetches the index each time and installs the newest version the specifier
 * names that suits the host, unless that one is installed already. When there
 * is no global default, the toolchain becomes the default.
 */
class ToolchainInstallCommandHandler : public CommandHandler {
public:
    /**
     * @brief Install the toolchain and report the outcome on standard error.
     *
     * @param ctx Invocation context. Its first positional is the specifier.
     * @return 0 on success, 1 when it reports a failure.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
