#include "project/FormattedFiles.h"

#include "project/Manifest.h"
#include "project/SourceCollector.h"

#include <algorithm>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Project {

namespace {

/// Directory the build writes into, which holds nothing to format.
constexpr std::string_view BuildDirectory = "build";

/// Directory the default layout keeps sources in.
constexpr std::string_view SourceDirectory = "src";

/// Directory the default layout keeps public headers in.
constexpr std::string_view IncludeDirectory = "include";

/// Extensions of the files below src/: sources, then headers.
constexpr auto SourceAndHeaderExtensions = Detail::concatenated(SourceExtensions, HeaderExtensions);

/**
 * Whether @p relative, a normalized path relative to the project root, is
 * build/ or below it.
 */
bool isBelowBuild(const std::filesystem::path& relative)
{
    return (! relative.empty()) && (*relative.begin() == BuildDirectory);
}

/**
 * Add to @p files every regular file below @p directory, relative to
 * @p projectRoot, whose extension is one of @p extensions. build/ is not
 * walked into.
 */
std::expected<void, SourceScanFailure> scanInto(const std::filesystem::path& projectRoot,
                                                const std::filesystem::path& directory,
                                                std::span<const std::string_view> extensions,
                                                std::vector<std::filesystem::path>& files)
{
    const std::filesystem::path walked = projectRoot / directory;
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::status(walked, ec);
    if (! std::filesystem::status_known(status)) {
        return std::unexpected(SourceScanFailure{ .directory = walked, .reason = ec.message() });
    }
    if (! std::filesystem::is_directory(status)) {
        return {};
    }

    std::filesystem::recursive_directory_iterator it(walked, ec);
    if (ec) {
        return std::unexpected(SourceScanFailure{ .directory = walked, .reason = ec.message() });
    }
    // The failure is read straight after the step that caused it, as in
    // collectSources(): a failed increment leaves the iterator at the end.
    for (const std::filesystem::recursive_directory_iterator end; it != end;) {
        const std::filesystem::path current = it->path();
        const std::filesystem::path relative = current.lexically_relative(projectRoot).lexically_normal();
        if (isBelowBuild(relative)) {
            it.disable_recursion_pending();
        } else if (it->is_regular_file(ec) && std::ranges::contains(extensions, current.extension().string())) {
            files.push_back(relative);
        }
        it.increment(ec);
        if (ec) {
            return std::unexpected(SourceScanFailure{ .directory = current, .reason = ec.message() });
        }
    }
    return {};
}

/// Every directory a target names in include-dirs, its own or its public ones.
std::vector<std::filesystem::path> includeDirectoriesOf(const std::vector<Target>& targets)
{
    std::vector<std::filesystem::path> directories;
    for (const Target& target : targets) {
        directories.insert(directories.end(), target.settings.includeDirectories.begin(), target.settings.includeDirectories.end());
        directories.insert(directories.end(), target.publicSettings.includeDirectories.begin(), target.publicSettings.includeDirectories.end());
    }
    return directories;
}

}  // anonymous namespace

std::expected<std::vector<std::filesystem::path>, SourceScanFailure> filesToFormat(const std::filesystem::path& projectRoot,
                                                                                   const std::vector<Target>& targets)
{
    std::vector<std::filesystem::path> files;
    if (auto scanned = scanInto(projectRoot, SourceDirectory, SourceAndHeaderExtensions, files); ! scanned.has_value()) {
        return std::unexpected(scanned.error());
    }
    if (auto scanned = scanInto(projectRoot, IncludeDirectory, HeaderExtensions, files); ! scanned.has_value()) {
        return std::unexpected(scanned.error());
    }
    for (const std::filesystem::path& directory : includeDirectoriesOf(targets)) {
        if (auto scanned = scanInto(projectRoot, directory.lexically_normal(), HeaderExtensions, files); ! scanned.has_value()) {
            return std::unexpected(scanned.error());
        }
    }
    for (const Target& target : targets) {
        const std::filesystem::path named = target.source.lexically_normal();
        if ((! named.empty()) && (! isBelowBuild(named))) {
            files.push_back(named);
        }
    }
    std::ranges::sort(files);
    const auto duplicates = std::ranges::unique(files);
    files.erase(duplicates.begin(), duplicates.end());
    return files;
}

}  // namespace scrap::Project
