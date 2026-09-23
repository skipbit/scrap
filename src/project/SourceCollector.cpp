#include "project/SourceCollector.h"

#include "project/Manifest.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace scrap::Project {

namespace {

/// Directory the default layout keeps sources in.
constexpr std::string_view SourceDirectory = "src";

/// Extensions a source file is recognised by.
constexpr std::array<std::string_view, 3> SourceExtensions{ ".cpp", ".cc", ".cxx" };

bool isSource(const std::filesystem::directory_entry& entry)
{
    std::error_code ec;
    if ((! entry.is_regular_file(ec)) || ec) {
        return false;
    }
    const std::string extension = entry.path().extension().string();
    return (std::ranges::find(SourceExtensions, extension) != SourceExtensions.end());
}

/**
 * Every source below src/, as normalized paths relative to the project root.
 *
 * A project without the directory has nothing to scan, which is not a
 * failure. A directory that exists and cannot be read is reported: leaving
 * its sources out would link an artifact from fewer files than the project
 * holds, and nothing later in the build would name what went missing.
 */
std::expected<std::vector<std::filesystem::path>, SourceScanFailure> scanSourceDirectory(const std::filesystem::path& projectRoot)
{
    const std::filesystem::path directory = projectRoot / SourceDirectory;
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::status(directory, ec);
    if (! std::filesystem::status_known(status)) {
        return std::unexpected(SourceScanFailure{ .directory = directory, .reason = ec.message() });
    }
    if (! std::filesystem::is_directory(status)) {
        return std::vector<std::filesystem::path>{};
    }

    std::vector<std::filesystem::path> sources;
    std::filesystem::recursive_directory_iterator it(directory, ec);
    if (ec) {
        return std::unexpected(SourceScanFailure{ .directory = directory, .reason = ec.message() });
    }
    // The failure is read straight after the step that caused it: an increment
    // that fails leaves the iterator at the end, so a check at the top of the
    // loop would never run and the walk would stop as though it had finished.
    for (const std::filesystem::recursive_directory_iterator end; it != end;) {
        const std::filesystem::path current = it->path();
        if (isSource(*it)) {
            sources.push_back(current.lexically_relative(projectRoot).lexically_normal());
        }
        it.increment(ec);
        if (ec) {
            return std::unexpected(SourceScanFailure{ .directory = current, .reason = ec.message() });
        }
    }
    std::ranges::sort(sources);
    return sources;
}

/**
 * The entry points of every target other than the one at the given index,
 * normalized so a path written with a "." component matches the file it names.
 */
std::vector<std::filesystem::path> otherEntryPoints(const std::vector<Target>& targets, const std::size_t index)
{
    std::vector<std::filesystem::path> entryPoints;
    for (std::size_t other = 0; other < targets.size(); ++other) {
        if (other != index) {
            entryPoints.push_back(targets[other].entryPoint.lexically_normal());
        }
    }
    return entryPoints;
}

}  // anonymous namespace

std::expected<std::vector<TargetSources>, SourceScanFailure> collectSources(const std::filesystem::path& projectRoot,
                                                                            const std::vector<Target>& targets)
{
    const auto scanned = scanSourceDirectory(projectRoot);
    if (! scanned.has_value()) {
        return std::unexpected(scanned.error());
    }

    std::vector<TargetSources> collected;
    collected.reserve(targets.size());
    for (std::size_t index = 0; index < targets.size(); ++index) {
        const Target& target = targets[index];
        const std::filesystem::path entryPoint = target.entryPoint.lexically_normal();
        const std::vector<std::filesystem::path> excluded = otherEntryPoints(targets, index);

        std::vector<std::filesystem::path> sources;
        for (const std::filesystem::path& source : *scanned) {
            if (std::ranges::find(excluded, source) == excluded.end()) {
                sources.push_back(source);
            }
        }
        if (std::ranges::find(sources, entryPoint) == sources.end()) {
            sources.push_back(entryPoint);
            std::ranges::sort(sources);
        }

        collected.push_back(TargetSources{ .target = target, .sources = std::move(sources) });
    }
    return collected;
}

}  // namespace scrap::Project
