#pragma once

#include <filesystem>
#include <vector>

namespace scrap::Command {

struct RuntimeEnvironment {
    /**
     * The directory the command was invoked in, which is where the search for
     * a project starts rather than its result: a command run below a project
     * root gets the subdirectory here. The directory that actually holds the
     * manifest is what scrap::Project::findProjectRoot() returns, and a
     * command that needs the project asks for it there. The name predates
     * that distinction and is due to be corrected when the project commands
     * start resolving a project.
     */
    std::filesystem::path projectRoot;
    std::vector<std::filesystem::path> searchPaths;
};

}  // namespace scrap::Command
