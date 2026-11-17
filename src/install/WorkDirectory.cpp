#include "install/WorkDirectory.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace scrap::Install {

std::expected<WorkDirectory, WorkDirectoryNotMade> WorkDirectory::below(const std::filesystem::path& parent, const std::string_view prefix)
{
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        return std::unexpected(WorkDirectoryNotMade{ .path = parent, .code = ec });
    }
    const std::string namePrefix = std::string{ prefix } + "-" + std::to_string(::getpid()) + "-";
    for (unsigned count = 0;; ++count) {
        std::filesystem::path candidate = parent / (namePrefix + std::to_string(count));
        if (std::filesystem::create_directory(candidate, ec)) {
            return WorkDirectory{ std::move(candidate) };
        }
        if (ec) {
            return std::unexpected(WorkDirectoryNotMade{ .path = candidate, .code = ec });
        }
    }
}

WorkDirectory::WorkDirectory(WorkDirectory&& other) noexcept
    : _path(std::exchange(other._path, {}))
{
}

WorkDirectory::~WorkDirectory()
{
    if (! _path.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(_path, ec);
    }
}

const std::filesystem::path& WorkDirectory::path() const
{
    return _path;
}

void WorkDirectory::keepOnly(const std::filesystem::path& kept)
{
    std::error_code ec;
    for (std::filesystem::directory_iterator it(_path, ec), end; (! ec) && (it != end); it.increment(ec)) {
        if (it->path() != kept) {
            std::error_code entryError;
            std::filesystem::remove_all(it->path(), entryError);
        }
    }
    _path.clear();
}

WorkDirectory::WorkDirectory(std::filesystem::path path)
    : _path(std::move(path))
{
}

}  // namespace scrap::Install
