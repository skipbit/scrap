#include "toolchain/ProgramSearch.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace scrap::Toolchain {

bool isExecutableFile(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::status(path, ec);
    if (ec || (! std::filesystem::is_regular_file(status))) {
        return false;
    }
    return (::access(path.c_str(), X_OK) == 0);
}

std::optional<std::filesystem::path> findOnSearchPaths(std::string_view name, const std::vector<std::filesystem::path>& searchPaths)
{
    for (const std::filesystem::path& directory : searchPaths) {
        std::filesystem::path candidate = directory / name;
        if (isExecutableFile(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> findProgram(std::string_view named, const std::vector<std::filesystem::path>& searchPaths)
{
    const std::filesystem::path path{ named };
    if (path.has_parent_path()) {
        return isExecutableFile(path) ? std::optional{ path } : std::nullopt;
    }
    return findOnSearchPaths(named, searchPaths);
}

std::filesystem::path absoluteProgramPath(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path absolute = std::filesystem::absolute(path, ec);
    if (ec) {
        return path;
    }
    return absolute;
}

}  // namespace scrap::Toolchain
