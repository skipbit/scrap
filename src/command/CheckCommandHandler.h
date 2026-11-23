#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap check [<path>...] [--release] [-j <jobs>]".
 *
 * Loads the project the paths, or the working directory when no path is
 * given, belong to, writes the commands scrap build would compile it with to
 * compile_commands.json, and analyzes with the clang-tidy found on PATH the
 * sources of it that the paths name, several at once. A named file that is
 * not a source of a target is left out with a warning. What clang-tidy wrote
 * for each file is passed on; the check fails when clang-tidy fails on a
 * file, as it does on a diagnostic it judges an error.
 */
class CheckCommandHandler : public CommandHandler {
public:
    /**
     * @brief Load the project and analyze its sources.
     *
     * @param ctx Invocation context. Its positionals are the paths.
     * @return 0 on success, 1 when it reports a failure.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
