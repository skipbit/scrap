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

/**
 * Whether @p path stays inside the project: relative, and climbing out of it
 * at no point. The same rule holds an entry point in a manifest, and it is
 * what keeps joining the path onto the project root from reaching past it.
 */
auto staysInsideProject(const std::filesystem::path& path) -> bool
{
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
        return false;
    }
    return std::ranges::none_of(path, [](const std::filesystem::path& part) {
        return part == "..";
    });
}

/**
 * Write one file of the template into @p root, or say why it could not be
 * written.
 */
auto writeTemplateFile(ProjectFileSystem& fileSystem,
                       const std::filesystem::path& root,
                       const TemplateFile& file) -> std::optional<CannotCreate>
{
    const std::filesystem::path target = root / file.path;
    if (! staysInsideProject(file.path)) {
        CannotCreate error = cannotCreate(target, std::make_error_code(std::errc::invalid_argument));
        error.reason = "the template file path leaves the project directory";
        return error;
    }

    const std::filesystem::path parent = target.parent_path();
    if (parent != root) {
        if (const std::error_code code = fileSystem.createDirectories(parent); code) {
            return cannotCreate(parent, code);
        }
    }
    if (const std::error_code code = fileSystem.writeNewFile(target, file.content); code) {
        return cannotCreate(target, code);
    }
    return std::nullopt;
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

    // The template runs once the project has a directory of its own, so a name
    // that is taken costs it nothing.
    const std::vector<TemplateFile> files = templateFiles(name);
    for (const TemplateFile& file : files) {
        std::optional<CannotCreate> failure = writeTemplateFile(fileSystem, root, file);
        if (failure.has_value()) {
            if (fileSystem.removeAll(root)) {
                failure->leftBehind = root;
            }
            return std::unexpected(CreateProjectError{std::move(*failure)});
        }
    }
    return root;
}

}  // namespace scrap::Project
