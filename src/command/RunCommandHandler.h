#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap run [<path>] [-- <args>...]".
 *
 * Loads the project that the path, or the working directory when no path is
 * given, belongs to, builds it, and starts its one executable with the
 * arguments after "--". The program runs in the working directory and at
 * the terminal: it shares the standard input, output and error of scrap.
 */
class RunCommandHandler : public CommandHandler {
public:
    /// What scrap run returns when the build fails and nothing is started.
    static constexpr int BuildFailedExitCode = 101;

    /**
     * @brief Build the project and run its executable.
     *
     * @param ctx Invocation context. Its first positional is the optional
     *   path, and its trailing arguments go to the program.
     * @return The program's exit code; 128 plus the signal when a signal
     *   stopped it; BuildFailedExitCode when the build fails; 1 when the
     *   project cannot be run for another reason.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
