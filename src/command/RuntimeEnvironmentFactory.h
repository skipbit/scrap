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
 * main()) are responsible for reading SCRAP_HOME, HOME, PATH and CXX and for
 * resolving the current working directory.
 *
 * scrapHome is SCRAP_HOME when it is not empty, otherwise ".scrap" below
 * HOME when HOME is not empty, otherwise nothing. A relative path is taken
 * against @p cwd, so scrapHome is absolute whenever @p cwd is: the same
 * relative SCRAP_HOME names a different directory in each directory a
 * command is run from.
 *
 * @param cwd       Current working directory, used as workingDirectory.
 * @param scrapHome Value of SCRAP_HOME, or empty if unset.
 * @param home      Value of HOME, or empty if unset.
 * @param pathEnv   Value of PATH, colon-separated. Empty segments (from
 *                  leading, trailing, or doubled colons) are skipped.
 * @param compilerEnv Value of CXX, or empty if unset, used as
 *                  preferredCompiler.
 * @return Constructed RuntimeEnvironment.
 */
[[nodiscard]] RuntimeEnvironment makeRuntimeEnvironment(const std::filesystem::path& cwd,
                                                        const std::string& scrapHome,
                                                        const std::string& home,
                                                        const std::string& pathEnv,
                                                        const std::string& compilerEnv);

}  // namespace scrap::Command
