#pragma once

#include <filesystem>
#include <vector>

namespace scrap::Project {

/**
 * @brief A path the command line names, relative to the project root.
 */
struct RequestedPath {
    std::filesystem::path path;
    bool isDirectory = false;  ///< Whether it names a directory rather than a file.
};

/**
 * @brief The files a command works on, and the named files it leaves out.
 */
struct FileSelection {
    std::vector<std::filesystem::path> selected;  ///< Sorted, each once.
    std::vector<std::filesystem::path> leftOut;   ///< Named files the command does not cover, in the order named, each once.
};

/**
 * @brief Narrow the files a command covers to the paths the command line
 *        names.
 *
 * A named directory selects the covered files below it, and a named file
 * selects itself when it is covered and is left out otherwise. Paths are
 * compared once normalized. Nothing named selects every covered file.
 *
 * @param covered Files the command covers, in any order, repeats allowed.
 * @param requested Paths the command line names.
 */
[[nodiscard]] FileSelection selectFiles(const std::vector<std::filesystem::path>& covered, const std::vector<RequestedPath>& requested);

}  // namespace scrap::Project
