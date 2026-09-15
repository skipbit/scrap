#include "project/ProjectCreator.h"

#include "project/TemplateFile.h"

#include <algorithm>
#include <cerrno>
#include <expected>  // NOLINT(misc-include-cleaner) — provides std::expected return type
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace scrap::Project {

namespace {

auto isAsciiLetter(const char ch) -> bool
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

auto isAsciiDigit(const char ch) -> bool
{
    return ch >= '0' && ch <= '9';
}

/**
 * Why a stream operation on @p file failed. The standard streams report a
 * cause only through errno, and only when a system call set it, so callers
 * clear errno before the operation and a failure that leaves it clear gets a
 * fixed reason and no error code.
 */
auto streamFailure(const std::filesystem::path& file) -> CannotCreate
{
    if (errno == 0) {
        return CannotCreate{
            .path = file, .reason = "the file could not be written", .code = {}, .leftBehind = std::nullopt};
    }
    const std::error_code code(errno, std::generic_category());
    return CannotCreate{.path = file, .reason = code.message(), .code = code, .leftBehind = std::nullopt};
}

/**
 * Write @p content to @p file, creating its parent directories.
 */
auto writeFile(const std::filesystem::path& file, std::string_view content) -> std::expected<void, CannotCreate>
{
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) {
        return std::unexpected(
            CannotCreate{.path = file.parent_path(), .reason = ec.message(), .code = ec, .leftBehind = std::nullopt});
    }

    errno = 0;
    std::ofstream output(file, std::ios::binary);
    if (! output.is_open()) {
        return std::unexpected(streamFailure(file));
    }
    errno = 0;
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.close();
    if (! output) {
        return std::unexpected(streamFailure(file));
    }
    return {};
}

}  // anonymous namespace

auto isValidProjectName(std::string_view name) -> bool
{
    if (name.empty() || name.size() > MaxProjectNameLength || ! isAsciiLetter(name.front())) {
        return false;
    }
    return std::ranges::all_of(name, [](const char ch) {
        return isAsciiLetter(ch) || isAsciiDigit(ch) || ch == '-' || ch == '_';
    });
}

auto createProject(const std::filesystem::path& parentDir,
                   std::string_view name,
                   const TemplateFiles& templateFiles) -> std::expected<std::filesystem::path, CreateProjectError>
{
    if (! isValidProjectName(name)) {
        return std::unexpected(CreateProjectError{InvalidProjectName{.name = std::string{name}}});
    }
    const std::vector<TemplateFile> files = templateFiles(name);

    std::error_code ec;
    std::filesystem::path root = std::filesystem::absolute(parentDir, ec);
    if (ec) {
        return std::unexpected(CreateProjectError{
            CannotCreate{.path = parentDir / name, .reason = ec.message(), .code = ec, .leftBehind = std::nullopt}});
    }
    root /= name;

    // create_directory reports an existing directory by returning false, and
    // any other existing entry through the error code.
    const bool created = std::filesystem::create_directory(root, ec);
    if (ec == std::errc::file_exists || (! ec && ! created)) {
        return std::unexpected(CreateProjectError{PathExists{.path = root}});
    }
    if (ec) {
        return std::unexpected(CreateProjectError{
            CannotCreate{.path = root, .reason = ec.message(), .code = ec, .leftBehind = std::nullopt}});
    }

    for (const TemplateFile& file : files) {
        auto written = writeFile(root / file.path, file.content);
        if (! written.has_value()) {
            CannotCreate error = std::move(written.error());
            std::error_code removeError;
            std::filesystem::remove_all(root, removeError);
            if (removeError) {
                error.leftBehind = root;
            }
            return std::unexpected(CreateProjectError{std::move(error)});
        }
    }
    return root;
}

}  // namespace scrap::Project
