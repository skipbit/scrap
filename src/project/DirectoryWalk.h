#pragma once

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <functional>
#include <string>

namespace scrap::Project {

/**
 * @brief A directory could not be read.
 */
struct SourceScanFailure {
    std::filesystem::path directory;  ///< Directory being read when it failed.
    std::string reason;               ///< The operating system's description of the failure.
};

/**
 * @brief What a walk does after visiting an entry.
 */
enum class WalkStep : std::uint8_t {
    Continue,  ///< Go on, into the entry if it is a directory.
    SkipBelow  ///< Go on without entering the entry.
};

/// Called for each entry with its path relative to the project root, normalized.
using WalkVisitor = std::function<WalkStep(const std::filesystem::directory_entry&, const std::filesystem::path&)>;

/**
 * @brief Visit every entry below a directory, however deep.
 *
 * A directory that does not exist, or is not a directory, holds nothing and is
 * not a failure. One that exists and cannot be examined or read is reported
 * with the path that failed.
 *
 * @param projectRoot Directory the relative paths are taken from.
 * @param directory Directory to walk, relative to @p projectRoot.
 * @param visit Called for each entry found.
 * @return Nothing, or the directory that could not be read.
 */
[[nodiscard]] std::expected<void, SourceScanFailure> walkDirectory(const std::filesystem::path& projectRoot,
                                                                   const std::filesystem::path& directory,
                                                                   const WalkVisitor& visit);

}  // namespace scrap::Project
