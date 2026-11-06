#pragma once

#include <filesystem>
#include <vector>

namespace scrap::Install {

/**
 * @brief Where the toolchain starts in what was unpacked into @p directory: an
 *        archive may hold it all in one directory of its own.
 */
[[nodiscard]] std::filesystem::path contentRoot(const std::filesystem::path& directory);

/**
 * @brief The directories below @p root, relative to it, that hold a C++ runtime
 *        library.
 */
[[nodiscard]] std::vector<std::filesystem::path> findRuntimeLibraryDirectories(const std::filesystem::path& root);

}  // namespace scrap::Install
