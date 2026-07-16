#pragma once

#include "command/RuntimeEnvironment.h"

#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief Build a RuntimeEnvironment from raw process inputs.
 *
 * Pure function: projectRoot and searchPaths are derived solely from the
 * arguments, without reading environment variables or touching the
 * filesystem. Callers (e.g. main()) are responsible for reading SCRAP_HOME
 * and PATH and for resolving the current working directory.
 *
 * @param cwd       Current working directory, used as projectRoot.
 * @param scrapHome Value of SCRAP_HOME, or empty if unset. When non-empty,
 *                  "<scrapHome>/bin" is prepended to searchPaths.
 * @param pathEnv   Value of PATH, colon-separated. Empty segments (from
 *                  leading, trailing, or doubled colons) are skipped.
 * @return Constructed RuntimeEnvironment.
 */
[[nodiscard]] auto makeRuntimeEnvironment(const std::filesystem::path& cwd,
                                          const std::string& scrapHome,
                                          const std::string& pathEnv) -> RuntimeEnvironment;

}  // namespace scrap::Command
