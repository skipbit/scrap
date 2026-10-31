#pragma once

#include <filesystem>
#include <optional>
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
     * The directory scrap keeps what it installs in, from SCRAP_HOME, or
     * nothing when it is not set.
     */
    std::optional<std::filesystem::path> scrapHome;

    /**
     * Where the system's own programs are found: the directories PATH lists,
     * in order, as given.
     */
    std::vector<std::filesystem::path> systemSearchPaths;

    /**
     * The compiler the environment asks for, from CXX, or empty when it says
     * nothing.
     */
    std::string preferredCompiler;
};

}  // namespace scrap::Command
