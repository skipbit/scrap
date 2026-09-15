#pragma once

#include <filesystem>
#include <vector>

namespace scrap::Command {

struct RuntimeEnvironment {
    /**
     * The directory the command was invoked in. A project command loads its
     * project by passing this directory, or a path taken against it, to
     * scrap::Project::loadProject().
     */
    std::filesystem::path workingDirectory;
    std::vector<std::filesystem::path> searchPaths;
};

}  // namespace scrap::Command
