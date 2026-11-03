#include "file/PosixFile.h"

#include <cerrno>
#include <cstddef>
#include <expected>  // IWYU pragma: keep
#include <fcntl.h>
#include <filesystem>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

namespace scrap::File {

namespace {

/// Permissions a written file is created with, before the process umask.
constexpr mode_t WrittenFileMode = 0644;

}  // anonymous namespace

std::error_code lastFailure()
{
    return { errno, std::generic_category() };
}

std::error_code writeAll(const int descriptor, std::string_view content)
{
    const char* data = content.data();
    std::size_t remaining = content.size();
    while (remaining > 0) {
        const ssize_t written = ::write(descriptor, data, remaining);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return lastFailure();
        }
        if (written == 0) {
            // POSIX allows a write of zero bytes to report nothing written,
            // which would leave the loop where it started.
            return std::make_error_code(std::errc::io_error);
        }
        data += written;
        remaining -= static_cast<std::size_t>(written);
    }
    return {};
}

std::expected<int, std::error_code> openNewFile(const std::filesystem::path& file)
{
    // O_EXCL fails when anything is at the path; O_NOFOLLOW states the same
    // for a final symbolic link on systems where O_EXCL alone would follow it.
    // NOLINTNEXTLINE(hicpp-signed-bitwise) - POSIX open() flag combination
    const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, WrittenFileMode);
    if (descriptor < 0) {
        return std::unexpected(lastFailure());
    }
    return descriptor;
}

}  // namespace scrap::File
