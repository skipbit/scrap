#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace scrap::Compile {

/**
 * @brief The command that collects the object files of one target into a
 *        static library.
 */
struct ArchiveCommand {
    std::string target;                  ///< The target it archives.
    std::filesystem::path directory;     ///< Where the command runs: the project root, absolute.
    std::filesystem::path output;        ///< The library, relative to the directory.
    std::vector<std::string> arguments;  ///< The command line, starting with the archiver.
};

}  // namespace scrap::Compile
