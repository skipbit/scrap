#pragma once

#include "command/CommandHandler.h"

namespace scrap::Command {

/**
 * @brief Handler for "scrap format [<path>...]".
 *
 * Loads the project the paths, or the working directory when no path is
 * given, belong to, and formats in place the files of it that the paths name
 * with the clang-format found on PATH. A named file the format does not cover
 * is left out with a warning. A file clang-format fails on is reported with
 * what clang-format wrote, and the other files are still formatted.
 */
class FormatCommandHandler : public CommandHandler {
public:
    /**
     * @brief Load the project and format its files.
     *
     * @param ctx Invocation context. Its positionals are the paths.
     * @return 0 on success, 1 when it reports a failure.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
