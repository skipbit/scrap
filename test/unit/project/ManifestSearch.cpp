#include "ManifestSearch.h"

#include "project/ProjectLocator.h"

#include <filesystem>
#include <optional>
#include <system_error>

namespace scrap::TestSupport {

std::optional<std::filesystem::path> manifestAbove(const std::filesystem::path& directory)
{
    std::error_code ec;
    for (std::filesystem::path current = directory;; current = current.parent_path()) {
        if (std::filesystem::is_regular_file(current / Project::ManifestFileName, ec)) {
            return current;
        }
        if (current.parent_path() == current) {
            return std::nullopt;
        }
    }
}

}  // namespace scrap::TestSupport
