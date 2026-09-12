#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace scrap::Project {

/// The file a directory is recognised as a project root by.
inline constexpr std::string_view ManifestFileName = "scrap.toml";

/**
 * @brief Find the project a directory belongs to.
 *
 * Walks up from @p startDir looking for a manifest, and stops at the first
 * directory that has one. A directory below the project root therefore still
 * resolves to the project, the same way version control tools behave.
 *
 * @param startDir Directory to start the search at.
 * @return The project root, or nothing if the search reached the filesystem
 *         root without finding a manifest.
 */
[[nodiscard]] auto findProjectRoot(const std::filesystem::path& startDir) -> std::optional<std::filesystem::path>;

}  // namespace scrap::Project
