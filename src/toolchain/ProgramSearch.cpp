#include "toolchain/ProgramSearch.h"

#include "process/ExecutableFile.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Toolchain {

std::optional<std::filesystem::path> findOnSearchPaths(std::string_view name, const std::vector<std::filesystem::path>& searchPaths)
{
    for (const std::filesystem::path& directory : searchPaths) {
        std::filesystem::path candidate = directory / name;
        if (Process::isExecutableFile(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> findProgram(std::string_view named, const std::vector<std::filesystem::path>& searchPaths)
{
    const std::filesystem::path path{ named };
    if (path.has_parent_path()) {
        return Process::isExecutableFile(path) ? std::optional{ path } : std::nullopt;
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

std::filesystem::path pathAsArgument(const std::filesystem::path& path)
{
    if (path.is_relative() && (path.native().starts_with('-') || path.native().starts_with('@'))) {
        return std::filesystem::path{ "." } / path;
    }
    return path;
}

}  // namespace scrap::Toolchain
