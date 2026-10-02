#include "toolchain/ProgramSearch.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace scrap::Toolchain {

namespace {

/**
 * Whether @p path names a file this process can run.
 *
 * The permission bits do not carry that on their own: a file only its owner
 * may run is not one another user can, and one whose owner bit is clear can
 * still be reached through its group. The system is asked instead, which is
 * the same question the build will ask when it runs the program.
 */
bool isExecutableFile(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::status(path, ec);
    if (ec || (! std::filesystem::is_regular_file(status))) {
        return false;
    }
    return (::access(path.c_str(), X_OK) == 0);
}

}  // anonymous namespace

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
