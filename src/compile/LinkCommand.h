#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace scrap::Compile {

/**
 * @brief The command that links the object files of one target into an
 *        executable.
 */
struct LinkCommand {
    std::string target;                  ///< The target it links.
    std::filesystem::path directory;     ///< Where the command runs: the project root, absolute.
    std::filesystem::path output;        ///< The executable, relative to the directory.
    std::vector<std::string> arguments;  ///< The command line, starting with the compiler.
};

}  // namespace scrap::Compile
