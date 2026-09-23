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
 * The path is normalised lexically. Commands go through loadProject(), which
 * resolves the start through the filesystem first.
 *
 * @param startDir Directory to start the search at.
 * @return The project root, absolute and without a trailing separator, or
 *         nothing if the search reached the filesystem root without finding
 *         a manifest.
 */
[[nodiscard]] std::optional<std::filesystem::path> findProjectRoot(const std::filesystem::path& startDir);

}  // namespace scrap::Project
