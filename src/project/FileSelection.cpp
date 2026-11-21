#include "project/FileSelection.h"

#include <algorithm>
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

/// Add to @p selected the files of @p covered at or below @p directory.
void selectBelow(const std::vector<std::filesystem::path>& covered, const std::filesystem::path& directory, std::vector<std::filesystem::path>& selected)
{
    std::ranges::copy_if(covered, std::back_inserter(selected), [&](const std::filesystem::path& file) {
        return isBelow(file, directory);
    });
}

}  // anonymous namespace

FileSelection selectFiles(const std::vector<std::filesystem::path>& covered, const std::vector<RequestedPath>& requested)
{
    if (requested.empty()) {
        return FileSelection{ .selected = covered, .leftOut = {} };
    }

    FileSelection selection;
    for (const RequestedPath& named : requested) {
        const std::filesystem::path path = normalized(named.path);
        if (named.isDirectory) {
            selectBelow(covered, path, selection.selected);
        } else if (std::binary_search(covered.begin(), covered.end(), path)) {
            selection.selected.push_back(path);
        } else if (! std::ranges::contains(selection.leftOut, path)) {
            selection.leftOut.push_back(path);
        }
    }
    std::ranges::sort(selection.selected);
    const auto duplicates = std::ranges::unique(selection.selected);
    selection.selected.erase(duplicates.begin(), duplicates.end());
    return selection;
}

}  // namespace scrap::Project
