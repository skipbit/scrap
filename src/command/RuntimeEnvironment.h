#pragma once

#include <filesystem>
#include <vector>

namespace scrap::Command {

struct RuntimeEnvironment {
    /**
     * The directory the command was invoked in. A project command starts its
     * search for the project here; the project root is what
     * scrap::Project::findProjectRoot() returns.
     */
    std::filesystem::path workingDirectory;
    std::vector<std::filesystem::path> searchPaths;
};

}  // namespace scrap::Command
