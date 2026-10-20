#pragma once

#include "command/CommandCatalog.h"
#include "command/HelpRenderer.h"

#include <span>
#include <string>

namespace scrap::Command {

/**
 * @brief Answer a request for help, from `scrap help` or from `--help`.
 *
 * Writes the global help without a target, and the usage of the command it
 * names otherwise: its first word names a top-level command and each next
 * word one of the commands below the last. A target that names no command,
 * including an empty name, is a usage error: it is reported on standard
 * error, as the words typed, with the command to run next.
 *
 * @param renderer Renders the help text.
 * @param catalog Commands to look the target up in.
 * @param target Words naming the command to show; none for the global help.
 * @return Exit code: 0 when help was written, 2 on a usage error.
 */
int showHelp(const HelpRenderer& renderer, const CommandCatalog& catalog, std::span<const std::string> target);

}  // namespace scrap::Command
