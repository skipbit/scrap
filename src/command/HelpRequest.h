#pragma once

#include "command/CommandCatalog.h"
#include "command/HelpRenderer.h"

#include <optional>
#include <string_view>

namespace scrap::Command {

/**
 * @brief Answer a request for help, from `scrap help` or from `--help`.
 *
 * Writes the global help without a target, and the usage of the top-level
 * command it names otherwise. A target that names no command, including an
 * empty one, is a usage error: it is reported on standard error with the
 * command to run next.
 *
 * @param renderer Renders the help text.
 * @param catalog Commands to look the target up in.
 * @param target Name of the command to show, if any.
 * @return Exit code: 0 when help was written, 2 on a usage error.
 */
int showHelp(const HelpRenderer& renderer, const CommandCatalog& catalog, std::optional<std::string_view> target);

}  // namespace scrap::Command
