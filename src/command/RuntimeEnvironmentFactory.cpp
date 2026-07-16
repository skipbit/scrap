#include "command/RuntimeEnvironmentFactory.h"

#include "command/RuntimeEnvironment.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * Build a RuntimeEnvironment from the given cwd, SCRAP_HOME, and PATH values.
 */
auto makeRuntimeEnvironment(const std::filesystem::path& cwd,
                            const std::string& scrapHome,
                            const std::string& pathEnv) -> RuntimeEnvironment
{
    RuntimeEnvironment env;
    env.projectRoot = cwd;

    if (! scrapHome.empty()) {
        env.searchPaths.push_back(std::filesystem::path(scrapHome) / "bin");
    }

    std::size_t start = 0;
    while (start <= pathEnv.size()) {
        auto separator = pathEnv.find(':', start);
        auto segment =
            (separator == std::string::npos) ? pathEnv.substr(start) : pathEnv.substr(start, separator - start);
        if (! segment.empty()) {
            env.searchPaths.push_back(std::filesystem::path(segment));
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }

    return env;
}

}  // namespace scrap::Command
