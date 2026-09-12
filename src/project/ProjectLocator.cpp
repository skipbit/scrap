#include "project/ProjectLocator.h"

#include <filesystem>
#include <optional>
#include <system_error>

namespace scrap::Project {

/**
 * Walk up from startDir to the filesystem root looking for a manifest.
 */
auto findProjectRoot(const std::filesystem::path& startDir) -> std::optional<std::filesystem::path>
{
    std::error_code ec;
    std::filesystem::path directory = std::filesystem::absolute(startDir, ec);
    if (ec) {
        directory = startDir;
    }
    directory = directory.lexically_normal();

    while (! directory.empty()) {
        if (std::filesystem::is_regular_file(directory / ManifestFileName, ec)) {
            return directory;
        }
        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory) {
            return std::nullopt;
        }
        directory = parent;
    }
    return std::nullopt;
}

}  // namespace scrap::Project
