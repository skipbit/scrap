#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief Whether @p path names a file this process can run.
 *
 * The permission bits do not carry that on their own: a file only its owner
 * may run is not one another user can, and one whose owner bit is clear can
 * still be reached through its group. The system is asked instead, which is
 * the same question the build will ask when it runs the program.
 */
[[nodiscard]] bool isExecutableFile(const std::filesystem::path& path);

/**
 * @brief The first of @p searchPaths holding a program named @p name that
 *        can be run, joined with the name.
 */
[[nodiscard]] std::optional<std::filesystem::path> findOnSearchPaths(std::string_view name,
                                                                     const std::vector<std::filesystem::path>& searchPaths);

/**
 * @brief The program @p named names.
 *
 * A path carrying a directory is read as it stands, since it names one
 * program rather than a program to look for; a bare name is looked for on
 * @p searchPaths.
 *
 * @return The program, or nothing when it cannot be run.
 */
[[nodiscard]] std::optional<std::filesystem::path> findProgram(std::string_view named,
                                                               const std::vector<std::filesystem::path>& searchPaths);

/**
 * @brief @p path made independent of the directory the command ran in.
 *
 * A later step running the program from elsewhere still names the same file.
 * A symbolic link stays as it was found, since a program reached through one,
 * such as clang++ or a ccache link, acts on the name it is run by. A path
 * that cannot be made absolute is kept as it was found.
 */
[[nodiscard]] std::filesystem::path absoluteProgramPath(const std::filesystem::path& path);

}  // namespace scrap::Toolchain
