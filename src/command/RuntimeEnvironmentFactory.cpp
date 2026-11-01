#include "command/RuntimeEnvironmentFactory.h"

#include "command/RuntimeEnvironment.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * The entries of a colon-separated list, in order, without the empty ones a
 * leading, trailing, or doubled colon produces.
 */
std::vector<std::filesystem::path> splitSearchPaths(const std::string& pathEnv)
{
    std::vector<std::filesystem::path> paths;
    std::size_t start = 0;
    while (start <= pathEnv.size()) {
        auto separator = pathEnv.find(':', start);
        auto segment = (separator == std::string::npos) ? pathEnv.substr(start) : pathEnv.substr(start, separator - start);
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

/**
 * The directory scrap keeps what it installs in: SCRAP_HOME, else ".scrap"
 * below HOME, taken against @p cwd; nothing when both are empty.
 */
std::optional<std::filesystem::path> resolveScrapHome(const std::filesystem::path& cwd,
                                                      const std::string& scrapHome,
                                                      const std::string& home)
{
    if (! scrapHome.empty()) {
        return cwd / scrapHome;
    }
    if (! home.empty()) {
        return cwd / home / ".scrap";
    }
    return std::nullopt;
}

}  // anonymous namespace

/**
 * Build a RuntimeEnvironment from the given cwd, SCRAP_HOME, HOME, PATH and
 * CXX values.
 */
RuntimeEnvironment makeRuntimeEnvironment(const std::filesystem::path& cwd,
                                          const std::string& scrapHome,
                                          const std::string& home,
                                          const std::string& pathEnv,
                                          const std::string& compilerEnv)
{
    RuntimeEnvironment env;
    env.workingDirectory = cwd;
    env.preferredCompiler = compilerEnv;
    env.systemSearchPaths = splitSearchPaths(pathEnv);
    env.scrapHome = resolveScrapHome(cwd, scrapHome, home);
    return env;
}

}  // namespace scrap::Command
