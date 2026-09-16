#include "project/driver/DiskProjectFileSystem.h"

#include <cerrno>
#include <expected>  // NOLINT(misc-include-cleaner) — provides std::expected return type
#include <filesystem>
#include <fstream>
#include <ios>
#include <string_view>
#include <system_error>

namespace scrap::Project {

namespace {

/**
 * The failure a stream left in errno. A stream reports a cause no other way,
 * and sets errno only when a system call failed, so a failure that leaves it
 * clear is reported as an I/O error.
 */
auto streamFailure() -> std::error_code
{
    if (errno == 0) {
        return std::make_error_code(std::errc::io_error);
    }
    return {errno, std::generic_category()};
}

}  // anonymous namespace

auto DiskProjectFileSystem::absolute(const std::filesystem::path& path) const
    -> std::expected<std::filesystem::path, std::error_code>
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
auto DiskProjectFileSystem::createDirectory(const std::filesystem::path& directory) -> std::error_code
{
    std::error_code ec;
    const bool created = std::filesystem::create_directory(directory, ec);
    if (! ec && ! created) {
        return std::make_error_code(std::errc::file_exists);
    }
    return ec;
}

auto DiskProjectFileSystem::createDirectories(const std::filesystem::path& directory) -> std::error_code
{
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    return ec;
}

auto DiskProjectFileSystem::writeNewFile(const std::filesystem::path& file,
                                         std::string_view content) -> std::error_code
{
    errno = 0;
    std::ofstream output(file, std::ios::binary);
    if (! output.is_open()) {
        return streamFailure();
    }
    errno = 0;
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.close();
    if (! output) {
        return streamFailure();
    }
    return {};
}

auto DiskProjectFileSystem::removeAll(const std::filesystem::path& directory) -> std::error_code
{
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
    return ec;
}

}  // namespace scrap::Project
