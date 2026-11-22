#include "project/FileSelection.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <vector>

namespace scrap::Project {

namespace {

/// @p path normalized, without a trailing separator.
std::filesystem::path normalized(const std::filesystem::path& path)
{
    std::filesystem::path normal = path.lexically_normal();
    if (normal.has_relative_path() && (! normal.has_filename())) {
        normal = normal.parent_path();
    }
    return normal;
}

/// Whether @p file is @p directory or below it; both are normalized.
bool isBelow(const std::filesystem::path& file, const std::filesystem::path& directory)
{
    const std::filesystem::path relative = file.lexically_relative(directory);
    return (! relative.empty()) && (*relative.begin() != "..");
}

/**
 * Add to @p selected the files of @p covered at or below @p directory, and
 * return whether there was any.
 */
bool selectBelow(const std::vector<std::filesystem::path>& covered, const std::filesystem::path& directory, std::vector<std::filesystem::path>& selected)
{
    const std::size_t before = selected.size();
    std::ranges::copy_if(covered, std::back_inserter(selected), [&](const std::filesystem::path& file) {
        return isBelow(file, directory);
    });
    return selected.size() != before;
}

/// Add @p path to @p paths unless it is there already.
void addOnce(std::vector<std::filesystem::path>& paths, const std::filesystem::path& path)
{
    if (! std::ranges::contains(paths, path)) {
        paths.push_back(path);
    }
}

}  // anonymous namespace

FileSelection selectFiles(const std::vector<std::filesystem::path>& covered, const std::vector<RequestedPath>& requested)
{
    FileSelection selection;
    if (requested.empty()) {
        selection.selected = covered;
    }
    for (const RequestedPath& named : requested) {
        const std::filesystem::path path = normalized(named.path);
        if (named.isDirectory) {
            if (! selectBelow(covered, path, selection.selected)) {
                addOnce(selection.emptyDirectories, path);
            }
        } else if (std::ranges::contains(covered, path)) {
            selection.selected.push_back(path);
        } else {
            addOnce(selection.leftOut, path);
        }
    }
    std::ranges::sort(selection.selected);
    const auto duplicates = std::ranges::unique(selection.selected);
    selection.selected.erase(duplicates.begin(), duplicates.end());
    return selection;
}

}  // namespace scrap::Project
