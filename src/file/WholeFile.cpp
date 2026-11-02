#include "file/WholeFile.h"

#include "file/StagedFile.h"

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

/// Permissions a written file is created with, before the process umask.
constexpr mode_t WrittenFileMode = 0644;

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

std::error_code writeNewFile(const std::filesystem::path& file, std::string_view content)
{
    const auto descriptor = openNewFile(file);
    if (! descriptor.has_value()) {
        return descriptor.error();
    }

    std::error_code failure = writeAll(*descriptor, content);
    if ((::close(*descriptor) != 0) && (! failure)) {
        failure = lastFailure();
    }
    if (failure) {
        ::unlink(file.c_str());
    }
    return failure;
}

std::error_code replaceFile(const std::filesystem::path& file, std::string_view content)
{
    auto staged = StagedFile::begin(file);
    if (! staged.has_value()) {
        return staged.error();
    }
    if (const std::error_code failure = staged->append(content)) {
        return failure;
    }
    return staged->commit();
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
