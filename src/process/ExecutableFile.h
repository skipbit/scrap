#pragma once

#include <filesystem>

namespace scrap::Process {

/**
 * @brief Whether @p path names a regular file this process can run.
 *
 * The permission bits do not carry that on their own: a file only its owner
 * may run is not one another user can, and one whose owner bit is clear can
 * still be reached through its group. The system is asked instead, which is
 * the same question it answers when the program is run.
 */
[[nodiscard]] bool isExecutableFile(const std::filesystem::path& path);

}  // namespace scrap::Process
