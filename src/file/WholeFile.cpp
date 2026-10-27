#include "file/WholeFile.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>  // IWYU pragma: keep
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

namespace scrap::File {

namespace {

/// Permissions a replaced file is created with, before the process umask.
constexpr mode_t WrittenFileMode = 0644;

/// How many staged names are tried when others are already taken.
constexpr int StagingAttempts = 16;

/**
 * The failure the last system call reported.
 */
std::error_code lastFailure()
{
    return { errno, std::generic_category() };
}

}  // anonymous namespace

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

std::error_code replaceFile(const std::filesystem::path& file, std::string_view content)
{
    const std::string prefix = file.string() + "." + std::to_string(::getpid()) + ".";
    for (int attempt = 0; attempt < StagingAttempts; ++attempt) {
        const std::filesystem::path staged = prefix + std::to_string(attempt) + ".tmp";
        // NOLINTNEXTLINE(hicpp-signed-bitwise) - POSIX open() flag combination
        const int descriptor = ::open(staged.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, WrittenFileMode);
        if (descriptor < 0) {
            if (errno == EEXIST) {
                continue;
            }
            return lastFailure();
        }

        std::error_code failure = writeAll(descriptor, content);
        if ((::close(descriptor) != 0) && (! failure)) {
            failure = lastFailure();
        }
        if ((! failure) && (::rename(staged.c_str(), file.c_str()) != 0)) {
            failure = lastFailure();
        }
        if (failure) {
            ::unlink(staged.c_str());
        }
        return failure;
    }
    return std::make_error_code(std::errc::file_exists);
}

std::expected<std::string, ReadFailure> readWholeFile(const std::filesystem::path& file)
{
    std::error_code ec;
    if (! std::filesystem::is_regular_file(file, ec)) {
        return std::unexpected(ReadFailure::CannotOpen);
    }
    const std::uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec) {
        return std::unexpected(ReadFailure::CannotRead);
    }

    std::ifstream input(file, std::ios::binary);
    if (! input.is_open()) {
        return std::unexpected(ReadFailure::CannotOpen);
    }
    if (size == 0) {
        return std::string{};
    }

    std::string text(static_cast<std::size_t>(size), '\0');
    input.read(text.data(), static_cast<std::streamsize>(size));
    if (static_cast<std::uintmax_t>(input.gcount()) != size) {
        return std::unexpected(ReadFailure::CannotRead);
    }
    return text;
}

}  // namespace scrap::File
