#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scrap::Compile {

/**
 * @brief The command that compiles one source into one object file.
 *
 * The compilation database records it and the build runs it, so an editor
 * reads the same command the compiler is given.
 */
struct CompileCommand {
    std::string target;                  ///< The target the object file is built for.
    std::filesystem::path directory;     ///< Where the command runs: the project root, absolute.
    std::filesystem::path file;          ///< The source, relative to the directory.
    std::filesystem::path output;        ///< The object file, relative to the directory.
    std::vector<std::string> arguments;  ///< The command line, starting with the compiler.
    /// The file the compiler lists what the compilation read in, relative to
    /// the directory, or nothing when the compiler is not asked for one.
    std::optional<std::filesystem::path> dependencyFile;
};

}  // namespace scrap::Compile
