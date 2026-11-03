#include "file/WholeFile.h"

#include "file/PosixFile.h"
#include "file/StagedFile.h"

#include <cstddef>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace scrap::File {

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
