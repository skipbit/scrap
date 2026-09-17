#pragma once

#include "command/RuntimeEnvironment.h"

#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief Build a RuntimeEnvironment from raw process inputs.
 *
 * Pure function: every field is derived solely from the arguments, without
 * reading environment variables or touching the filesystem. Callers (e.g.
 * main()) are responsible for reading SCRAP_HOME, PATH and CXX and for
 * resolving the current working directory.
 *
 * @param cwd       Current working directory, used as workingDirectory.
 * @param scrapHome Value of SCRAP_HOME, or empty if unset. When non-empty,
 *                  "<scrapHome>/bin" is prepended to searchPaths, and to
 *                  searchPaths alone: systemSearchPaths holds PATH as given.
 * @param pathEnv   Value of PATH, colon-separated. Empty segments (from
 *                  leading, trailing, or doubled colons) are skipped.
 * @param compilerEnv Value of CXX, or empty if unset, used as
 *                  preferredCompiler.
 * @return Constructed RuntimeEnvironment.
 */
[[nodiscard]] auto makeRuntimeEnvironment(const std::filesystem::path& cwd,
                                          const std::string& scrapHome,
                                          const std::string& pathEnv,
                                          const std::string& compilerEnv) -> RuntimeEnvironment;

}  // namespace scrap::Command
