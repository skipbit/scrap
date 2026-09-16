#include "project/ProjectCreator.h"

#include "project/ProjectFileSystem.h"
#include "project/TemplateFile.h"

#include <algorithm>
#include <expected>  // NOLINT(misc-include-cleaner) — provides std::expected return type
#include <filesystem>
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
 * Build the error for a path the file system refused.
 */
auto cannotCreate(const std::filesystem::path& path, const std::error_code& code) -> CannotCreate
{
    return CannotCreate{.path = path, .reason = code.message(), .code = code, .leftBehind = std::nullopt};
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

auto createProject(ProjectFileSystem& fileSystem,
                   const std::filesystem::path& parentDir,
                   std::string_view name,
                   const TemplateFiles& templateFiles) -> std::expected<std::filesystem::path, CreateProjectError>
{
    if (! isValidProjectName(name)) {
        return std::unexpected(CreateProjectError{InvalidProjectName{.name = std::string{name}}});
    }
    const std::vector<TemplateFile> files = templateFiles(name);

    const auto parent = fileSystem.absolute(parentDir);
    if (! parent.has_value()) {
        return std::unexpected(CreateProjectError{cannotCreate(parentDir / name, parent.error())});
    }
    std::filesystem::path root = *parent / name;

    if (const std::error_code code = fileSystem.createDirectory(root); code) {
        if (code == std::errc::file_exists) {
            return std::unexpected(CreateProjectError{PathExists{.path = root}});
        }
        return std::unexpected(CreateProjectError{cannotCreate(root, code)});
    }

    for (const TemplateFile& file : files) {
        const std::filesystem::path target = root / file.path;
        std::error_code code = fileSystem.createDirectories(target.parent_path());
        std::filesystem::path failed = target.parent_path();
        if (! code) {
            code = fileSystem.writeNewFile(target, file.content);
            failed = target;
        }
        if (code) {
            CannotCreate error = cannotCreate(failed, code);
            if (fileSystem.removeAll(root)) {
                error.leftBehind = root;
            }
            return std::unexpected(CreateProjectError{std::move(error)});
        }
    }
    return root;
}

}  // namespace scrap::Project
