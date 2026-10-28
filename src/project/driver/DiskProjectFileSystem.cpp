#include "project/driver/DiskProjectFileSystem.h"

#include "file/WholeFile.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string_view>
#include <system_error>

namespace scrap::Project {

std::expected<std::filesystem::path, std::error_code> DiskProjectFileSystem::absolute(const std::filesystem::path& path) const
{
    std::error_code ec;
    std::filesystem::path resolved = std::filesystem::absolute(path, ec);
    if (ec) {
        return std::unexpected(ec);
    }
    return resolved;
}

/**
 * create_directory reports an existing directory by returning false, and any
 * other existing entry through the error code, so both become file_exists.
 */
std::error_code DiskProjectFileSystem::createDirectory(const std::filesystem::path& directory)
{
    std::error_code ec;
    const bool created = std::filesystem::create_directory(directory, ec);
    if ((! ec) && (! created)) {
        return std::make_error_code(std::errc::file_exists);
    }
    return ec;
}

std::error_code DiskProjectFileSystem::createDirectories(const std::filesystem::path& directory)
{
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    return ec;
}

std::error_code DiskProjectFileSystem::writeNewFile(const std::filesystem::path& file, std::string_view content)
{
    return File::writeNewFile(file, content);
}

std::error_code DiskProjectFileSystem::removeAll(const std::filesystem::path& directory)
{
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
    return ec;
}

}  // namespace scrap::Project
