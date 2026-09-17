#include "command/RuntimeEnvironmentFactory.h"

#include "command/RuntimeEnvironment.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * The entries of a colon-separated list, in order, without the empty ones a
 * leading, trailing, or doubled colon produces.
 */
auto splitSearchPaths(const std::string& pathEnv) -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths;
    std::size_t start = 0;
    while (start <= pathEnv.size()) {
        auto separator = pathEnv.find(':', start);
        auto segment =
            (separator == std::string::npos) ? pathEnv.substr(start) : pathEnv.substr(start, separator - start);
        if (! segment.empty()) {
            paths.emplace_back(segment);
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    return paths;
}

}  // anonymous namespace

/**
 * Build a RuntimeEnvironment from the given cwd, SCRAP_HOME, PATH and CXX
 * values.
 */
auto makeRuntimeEnvironment(const std::filesystem::path& cwd,
                            const std::string& scrapHome,
                            const std::string& pathEnv,
                            const std::string& compilerEnv) -> RuntimeEnvironment
{
    RuntimeEnvironment env;
    env.workingDirectory = cwd;
    env.preferredCompiler = compilerEnv;
    env.systemSearchPaths = splitSearchPaths(pathEnv);

    if (! scrapHome.empty()) {
        env.searchPaths.emplace_back(std::filesystem::path(scrapHome) / "bin");
    }
    env.searchPaths.insert(env.searchPaths.end(), env.systemSearchPaths.begin(), env.systemSearchPaths.end());

    return env;
}

}  // namespace scrap::Command
