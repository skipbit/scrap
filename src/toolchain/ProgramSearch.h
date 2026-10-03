#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace scrap::Toolchain {

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
