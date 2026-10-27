#include "project/SourceCollector.h"

#include "project/Manifest.h"

#include <algorithm>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace scrap::Project {

namespace {

/// Directory the default layout keeps sources in.
constexpr std::string_view SourceDirectory = "src";

std::string lowerCase(std::string text)
{
    for (char& ch : text) {
        if ((ch >= 'A') && (ch <= 'Z')) {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

bool isSource(const std::filesystem::directory_entry& entry)
{
    std::error_code ec;
    return entry.is_regular_file(ec) && std::ranges::contains(SourceExtensions, entry.path().extension().string());
}

/**
 * Why the scan leaves @p entry out although it looks like a source, or
 * nothing when it is a source or does not look like one.
 */
std::optional<LeftOutReason> leftOutReason(const std::filesystem::directory_entry& entry)
{
    std::error_code ec;
    if (entry.is_symlink(ec) && entry.is_directory(ec)) {
        return LeftOutReason::DirectoryLink;
    }
    const std::string extension = entry.path().extension().string();
    const std::string lower = lowerCase(extension);
    if (! std::ranges::contains(SourceExtensions, lower)) {
        return std::nullopt;
    }
    if (entry.is_symlink(ec) && (! entry.exists(ec))) {
        return LeftOutReason::BrokenLink;
    }
    if ((extension != lower) && entry.is_regular_file(ec)) {
        return LeftOutReason::ExtensionSpelling;
    }
    return std::nullopt;
}

/// What the scan of src/ found.
struct ScannedSources {
    std::vector<std::filesystem::path> sources;  ///< Relative to the project root, normalized and sorted.
    std::vector<LeftOutEntry> leftOut;           ///< Sorted by path.
};

/**
 * Every source below src/, as normalized paths relative to the project root,
 * and what looks like a source but is left out.
 *
 * A project without the directory has nothing to scan, which is not a
 * failure. A directory that exists and cannot be read is reported: leaving
 * its sources out would link an artifact from fewer files than the project
 * holds, and nothing later in the build would name what went missing.
 */
std::expected<ScannedSources, SourceCollectionError> scanSourceDirectory(const std::filesystem::path& projectRoot)
{
    const std::filesystem::path directory = projectRoot / SourceDirectory;
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::status(directory, ec);
    if (! std::filesystem::status_known(status)) {
        return std::unexpected(SourceScanFailure{ .directory = directory, .reason = ec.message() });
    }
    if (! std::filesystem::is_directory(status)) {
        return ScannedSources{};
    }

    ScannedSources scanned;
    std::filesystem::recursive_directory_iterator it(directory, ec);
    if (ec) {
        return std::unexpected(SourceScanFailure{ .directory = directory, .reason = ec.message() });
    }
    // The failure is read straight after the step that caused it: an increment
    // that fails leaves the iterator at the end, so a check at the top of the
    // loop would never run and the walk would stop as though it had finished.
    for (const std::filesystem::recursive_directory_iterator end; it != end;) {
        const std::filesystem::path current = it->path();
        const std::filesystem::path relative = current.lexically_relative(projectRoot).lexically_normal();
        if (isSource(*it)) {
            scanned.sources.push_back(relative);
        } else if (const auto reason = leftOutReason(*it)) {
            scanned.leftOut.push_back(LeftOutEntry{ .path = relative, .reason = *reason });
        }
        it.increment(ec);
        if (ec) {
            return std::unexpected(SourceScanFailure{ .directory = current, .reason = ec.message() });
        }
    }
    std::ranges::sort(scanned.sources);
    std::ranges::sort(scanned.leftOut, {}, &LeftOutEntry::path);
    return scanned;
}

/**
 * The entry points of every executable, normalized so a path written with a
 * "." component matches the file it names.
 */
std::vector<std::filesystem::path> entryPointsOf(const std::vector<Target>& targets)
{
    std::vector<std::filesystem::path> entryPoints;
    for (const Target& target : targets) {
        if (target.kind == TargetKind::Executable) {
            entryPoints.push_back(target.source.lexically_normal());
        }
    }
    return entryPoints;
}

/**
 * The scanned sources except the entry points other than @p named, with
 * @p named inserted in order when it is not among them. @p scanned is sorted,
 * which the insertion relies on.
 */
std::vector<std::filesystem::path> sourcesExcept(const std::vector<std::filesystem::path>& scanned,
                                                 const std::vector<std::filesystem::path>& entryPoints,
                                                 const std::filesystem::path& named)
{
    std::vector<std::filesystem::path> sources;
    for (const std::filesystem::path& source : scanned) {
        if ((source == named) || (! std::ranges::contains(entryPoints, source))) {
            sources.push_back(source);
        }
    }
    if (! named.empty()) {
        const auto at = std::lower_bound(sources.begin(), sources.end(), named);
        if ((at == sources.end()) || (*at != named)) {
            sources.insert(at, named);
        }
    }
    return sources;
}

/**
 * The scanned source that @p named reaches under another path, as the file
 * system decides. A declared file that does not exist is the same file as
 * none of them; the compiler reports it.
 */
std::optional<std::filesystem::path> sameFileAmong(const std::filesystem::path& projectRoot,
                                                   const std::vector<std::filesystem::path>& scanned,
                                                   const std::filesystem::path& named)
{
    if (named.empty() || (std::ranges::find(scanned, named) != scanned.end())) {
        return std::nullopt;
    }
    for (const std::filesystem::path& source : scanned) {
        std::error_code ec;
        if (std::filesystem::equivalent(projectRoot / source, projectRoot / named, ec)) {
            return source;
        }
    }
    return std::nullopt;
}

}  // anonymous namespace

std::expected<CollectedSources, SourceCollectionError> collectSources(const std::filesystem::path& projectRoot, const std::vector<Target>& targets)
{
    auto scanned = scanSourceDirectory(projectRoot);
    if (! scanned.has_value()) {
        return std::unexpected(scanned.error());
    }

    const bool hasLibrary = std::ranges::contains(targets, TargetKind::Library, &Target::kind);
    const std::vector<std::filesystem::path> entryPoints = entryPointsOf(targets);
    CollectedSources collected;
    collected.targets.reserve(targets.size());
    for (const Target& target : targets) {
        const std::filesystem::path named = target.source.lexically_normal();
        if (const auto found = sameFileAmong(projectRoot, scanned->sources, named)) {
            return std::unexpected(SourceNamedTwice{ .declared = named, .found = *found });
        }
        std::vector<std::filesystem::path> sources;
        if (hasLibrary && (target.kind == TargetKind::Executable)) {
            sources.push_back(named);
        } else {
            sources = sourcesExcept(scanned->sources, entryPoints, named);
        }
        // A declared file is built, so it is not listed as left out.
        std::erase_if(scanned->leftOut, [&](const LeftOutEntry& entry) {
            return entry.path == named;
        });
        collected.targets.push_back(TargetSources{ .target = target, .sources = std::move(sources) });
    }
    collected.leftOut = std::move(scanned->leftOut);
    return collected;
}

}  // namespace scrap::Project
