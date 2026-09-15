#include "project/ProjectCreator.h"

#include "project/DefaultTemplate.h"

#include <algorithm>
#include <cerrno>
#include <expected>  // NOLINT(misc-include-cleaner) — provides std::expected return type
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
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
 * The failure a stream left in errno, which the standard streams report no
 * other way.
 */
auto lastSystemError() -> std::string
{
    return std::error_code(errno, std::generic_category()).message();
}

/**
 * Write @p content to @p file, creating its parent directories.
 */
auto writeFile(const std::filesystem::path& file, std::string_view content) -> std::expected<void, CreateProjectError>
{
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) {
        return std::unexpected(CreateProjectError{CannotCreate{.path = file.parent_path(), .reason = ec.message()}});
    }

    std::ofstream output(file, std::ios::binary);
    if (! output.is_open()) {
        return std::unexpected(CreateProjectError{CannotCreate{.path = file, .reason = lastSystemError()}});
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.close();
    if (! output) {
        return std::unexpected(CreateProjectError{CannotCreate{.path = file, .reason = lastSystemError()}});
    }
    return {};
}

}  // anonymous namespace

auto isValidProjectName(std::string_view name) -> bool
{
    if (name.empty() || ! isAsciiLetter(name.front())) {
        return false;
    }
    return std::ranges::all_of(name, [](const char ch) {
        return isAsciiLetter(ch) || isAsciiDigit(ch) || ch == '-' || ch == '_';
    });
}

auto createProject(const std::filesystem::path& parentDir,
                   std::string_view name,
                   const std::vector<TemplateFile>& files) -> std::expected<std::filesystem::path, CreateProjectError>
{
    if (! isValidProjectName(name)) {
        return std::unexpected(CreateProjectError{InvalidProjectName{.name = std::string{name}}});
    }

    std::error_code ec;
    std::filesystem::path root = std::filesystem::absolute(parentDir, ec);
    if (ec) {
        return std::unexpected(CreateProjectError{CannotCreate{.path = parentDir / name, .reason = ec.message()}});
    }
    root /= name;

    // create_directory reports an existing directory by returning false, and
    // any other existing entry through the error code.
    const bool created = std::filesystem::create_directory(root, ec);
    if (ec == std::errc::file_exists || (! ec && ! created)) {
        return std::unexpected(CreateProjectError{PathExists{.path = root}});
    }
    if (ec) {
        return std::unexpected(CreateProjectError{CannotCreate{.path = root, .reason = ec.message()}});
    }

    for (const TemplateFile& file : files) {
        auto written = writeFile(root / file.path, file.content);
        if (! written.has_value()) {
            std::error_code ignored;
            std::filesystem::remove_all(root, ignored);
            return std::unexpected(written.error());
        }
    }
    return root;
}

}  // namespace scrap::Project
