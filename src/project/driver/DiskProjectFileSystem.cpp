#include "project/driver/DiskProjectFileSystem.h"

#include <cerrno>
#include <cstddef>
#include <expected>  // NOLINT(misc-include-cleaner) — provides std::expected return type
#include <fcntl.h>
#include <filesystem>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

namespace scrap::Project {

namespace {

/// Permissions a new project file is created with, before the process umask.
constexpr mode_t NewFileMode = 0644;

/**
 * The failure the last system call reported.
 */
auto lastFailure() -> std::error_code
{
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

/**
 * Create the file and write it in one pass over the content.
 *
 * O_EXCL makes the call fail when anything is at the path, which keeps an
 * entry placed there after the project directory was created, symbolic link
 * included, from being followed or truncated. O_NOFOLLOW states the same for
 * the final component on systems where O_EXCL alone would follow it.
 */
auto DiskProjectFileSystem::writeNewFile(const std::filesystem::path& file,
                                         std::string_view content) -> std::error_code
{
    // NOLINTNEXTLINE(hicpp-signed-bitwise) — POSIX open() flag combination
    const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, NewFileMode);
    if (descriptor < 0) {
        return lastFailure();
    }

    const char* data = content.data();
    std::size_t remaining = content.size();
    while (remaining > 0) {
        const ssize_t written = ::write(descriptor, data, remaining);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            const std::error_code failure = lastFailure();
            ::close(descriptor);
            return failure;
        }
        data += written;
        remaining -= static_cast<std::size_t>(written);
    }

    if (::close(descriptor) != 0) {
        return lastFailure();
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
