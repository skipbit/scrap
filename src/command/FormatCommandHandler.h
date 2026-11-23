#pragma once

#include "command/CommandHandler.h"

#include <string_view>

namespace scrap::Command {

/// The option that checks the files instead of formatting them.
inline constexpr std::string_view CheckOption = "check";

/**
 * @brief Handler for "scrap format [<path>...] [--check]".
 *
 * Loads the project the paths, or the working directory when no path is
 * given, belong to, and formats in place the files of it that the paths name
 * with the clang-format found on PATH. A named file the format does not cover
 * is left out with a warning. A file clang-format fails on is reported with
 * what clang-format wrote, and the other files are still formatted. With
 * CheckOption, the files stay as they are, and those formatting would
 * change are reported.
 */
class FormatCommandHandler : public CommandHandler {
public:
    /**
     * @brief Load the project and format its files, or check them.
     *
     * @param ctx Invocation context. Its positionals are the paths.
     * @return 0 on success, 1 when it reports a failure or, with
     *         CheckOption, a file formatting would change.
     */
    int execute(const InvocationContext& ctx) override;
};

}  // namespace scrap::Command
