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

RuntimeEnvironment makeRuntimeEnvironment(const ProcessInputs& inputs)
{
    RuntimeEnvironment env;
    env.workingDirectory = inputs.workingDirectory;
    env.preferredCompiler = inputs.compiler;
    env.systemSearchPaths = splitSearchPaths(inputs.path);
    env.scrapHome = resolveScrapHome(inputs.workingDirectory, inputs.scrapHome, inputs.home);
    return env;
}

}  // namespace scrap::Command
