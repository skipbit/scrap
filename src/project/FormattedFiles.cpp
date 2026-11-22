#include "project/FormattedFiles.h"

#include "project/DirectoryWalk.h"
#include "project/FileKinds.h"
#include "project/Manifest.h"

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
 * Whether @p path, once its links are resolved, is @p root or below it.
 * @p root is resolved already. A path that cannot be resolved counts as
 * inside, so the walk reports why.
 */
bool resolvesInside(const std::filesystem::path& path, const std::filesystem::path& root)
{
    std::error_code ec;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return true;
    }
    const std::filesystem::path relative = resolved.lexically_relative(root);
    return (! relative.empty()) && (*relative.begin() != "..");
}

/// Whether @p path is a symbolic link, which a format would replace with a copy.
bool isLink(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_symlink(std::filesystem::symlink_status(path, ec));
}

/// A directory to walk and the extensions of the files to take from it.
struct ScanTarget {
    std::filesystem::path directory;
    std::span<const std::string_view> extensions;
};

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
    return walkDirectory(projectRoot, directory, [&](const std::filesystem::directory_entry& entry, const std::filesystem::path& relative) {
        if (isBelowBuild(relative)) {
            return WalkStep::SkipBelow;
        }
        std::error_code ec;
        if ((! entry.is_symlink(ec)) && entry.is_regular_file(ec) && std::ranges::contains(extensions, entry.path().extension().string())) {
            files.push_back(relative);
        }
        return WalkStep::Continue;
    });
}

/// What to scan, in the order to scan it: src/, include/, then every include-dirs entry, a target's own before its public ones.
std::vector<ScanTarget> scanTargetsOf(const std::vector<Target>& targets)
{
    std::vector<ScanTarget> scans{ { .directory = SourceDirectory, .extensions = SourceAndHeaderExtensions },
                                   { .directory = IncludeDirectory, .extensions = HeaderExtensions } };
    for (const Target& target : targets) {
        for (const auto* settings : { &target.settings, &target.publicSettings }) {
            for (const std::filesystem::path& directory : settings->includeDirectories) {
                scans.push_back({ .directory = directory.lexically_normal(), .extensions = HeaderExtensions });
            }
        }
    }
    return scans;
}

/// Add to @p files every file a target names in src that a format may rewrite.
void addNamedFiles(const std::filesystem::path& projectRoot,
                   const std::filesystem::path& resolvedRoot,
                   const std::vector<Target>& targets,
                   std::vector<std::filesystem::path>& files)
{
    for (const Target& target : targets) {
        const std::filesystem::path named = target.source.lexically_normal();
        if (named.empty() || isBelowBuild(named) || isLink(projectRoot / named) || (! resolvesInside(projectRoot / named, resolvedRoot))) {
            continue;
        }
        files.push_back(named);
    }
}

}  // anonymous namespace

std::expected<std::vector<std::filesystem::path>, SourceScanFailure> filesToFormat(const std::filesystem::path& projectRoot,
                                                                                   const std::vector<Target>& targets)
{
    std::error_code ec;
    const std::filesystem::path resolvedRoot = std::filesystem::weakly_canonical(projectRoot, ec);
    std::vector<std::filesystem::path> files;
    for (const ScanTarget& scan : scanTargetsOf(targets)) {
        if (! resolvesInside(projectRoot / scan.directory, resolvedRoot)) {
            continue;
        }
        if (auto scanned = scanInto(projectRoot, scan.directory, scan.extensions, files); ! scanned.has_value()) {
            return std::unexpected(scanned.error());
        }
    }
    addNamedFiles(projectRoot, resolvedRoot, targets, files);
    std::ranges::sort(files);
    const auto duplicates = std::ranges::unique(files);
    files.erase(duplicates.begin(), duplicates.end());
    return files;
}

}  // namespace scrap::Project
