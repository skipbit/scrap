#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace scrap::Command {

struct RuntimeEnvironment {
    /**
     * The directory the command was invoked in. A project command loads its
     * project by passing this directory, or a path taken against it, to
     * scrap::Project::loadProject().
     */
    std::filesystem::path workingDirectory;

    /**
     * Where scrap looks for the commands it dispatches to: its own directory
     * first, then PATH.
     */
    std::vector<std::filesystem::path> searchPaths;

    /**
     * Where the system's own programs are found: PATH alone. Kept apart from
     * searchPaths so that a program scrap installed into its own directory is
     * never reported as one the system provides.
     */
    std::vector<std::filesystem::path> systemSearchPaths;

    /**
     * The compiler the environment asks for, from CXX, or empty when it says
     * nothing.
     */
    std::string preferredCompiler;
};

}  // namespace scrap::Command
