#include "file/StagedFile.h"

#include "file/PosixFile.h"

#include <cstdio>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace scrap::File {

namespace {

/// How many staged names are tried when others are already taken.
constexpr int StagingAttempts = 16;

}  // anonymous namespace

std::expected<StagedFile, std::error_code> StagedFile::begin(const std::filesystem::path& destination)
{
    const std::string prefix = destination.string() + "." + std::to_string(::getpid()) + ".";
    for (int attempt = 0; attempt < StagingAttempts; ++attempt) {
        std::filesystem::path staged = prefix + std::to_string(attempt) + ".tmp";
        auto descriptor = openNewFile(staged);
        if (descriptor.has_value()) {
            return StagedFile{ destination, std::move(staged), *descriptor };
        }
        if (descriptor.error() != std::errc::file_exists) {
            return std::unexpected(descriptor.error());
        }
    }
    return std::unexpected(std::make_error_code(std::errc::file_exists));
}

StagedFile::StagedFile(std::filesystem::path destination, std::filesystem::path staged, const int descriptor)
    : _destination(std::move(destination))
    , _staged(std::move(staged))
    , _descriptor(descriptor)
{
}

StagedFile::StagedFile(StagedFile&& other) noexcept
    : _destination(std::move(other._destination))
    , _staged(std::move(other._staged))
    , _descriptor(std::exchange(other._descriptor, -1))
{
}

StagedFile& StagedFile::operator=(StagedFile&& other) noexcept
{
    if (this != &other) {
        discard();
        _destination = std::move(other._destination);
        _staged = std::move(other._staged);
        _descriptor = std::exchange(other._descriptor, -1);
    }
    return *this;
}

StagedFile::~StagedFile()
{
    discard();
}

std::error_code StagedFile::append(std::string_view content) const
{
    return writeAll(_descriptor, content);
}

std::error_code StagedFile::commit()
{
    if (_descriptor < 0) {
        // The staged name is no longer this object's to remove.
        return std::make_error_code(std::errc::bad_file_descriptor);
    }
    if (::close(std::exchange(_descriptor, -1)) != 0) {
        const std::error_code failure = lastFailure();
        ::unlink(_staged.c_str());
        return failure;
    }
    if (::rename(_staged.c_str(), _destination.c_str()) != 0) {
        const std::error_code failure = lastFailure();
        ::unlink(_staged.c_str());
        return failure;
    }
    return {};
}

void StagedFile::discard() noexcept
{
    if (_descriptor >= 0) {
        ::close(std::exchange(_descriptor, -1));
        ::unlink(_staged.c_str());
    }
}

}  // namespace scrap::File
