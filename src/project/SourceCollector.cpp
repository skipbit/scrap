#include "project/SourceCollector.h"

#include "project/Manifest.h"

#include <algorithm>
#include <array>
#include <cstddef>
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
constexpr std::array<std::string_view, 3> SourceExtensions{".cpp", ".cc", ".cxx"};

auto isSource(const std::filesystem::directory_entry& entry) -> bool
{
    std::error_code ec;
    if (! entry.is_regular_file(ec) || ec) {
        return false;
    }
    const std::string extension = entry.path().extension().string();
    return std::ranges::find(SourceExtensions, extension) != SourceExtensions.end();
}

/**
 * Every source below src/, as paths relative to the project root.
 *
 * A directory that cannot be read yields what was found before it rather than
 * a failure: the build reports a source it cannot compile, which is where the
 * compiler's own diagnostic belongs.
 */
auto scanSourceDirectory(const std::filesystem::path& projectRoot) -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> sources;
    std::error_code ec;
    const std::filesystem::path directory = projectRoot / SourceDirectory;
    for (std::filesystem::recursive_directory_iterator it(directory, ec), end; ! ec && it != end; it.increment(ec)) {
        if (isSource(*it)) {
            sources.push_back(it->path().lexically_relative(projectRoot));
        }
    }
    std::ranges::sort(sources);
    return sources;
}

/**
 * The entry points of every target other than the one at @p index.
 */
auto otherEntryPoints(const std::vector<Target>& targets, const std::size_t index) -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> entryPoints;
    for (std::size_t other = 0; other < targets.size(); ++other) {
        if (other != index) {
            entryPoints.push_back(targets[other].entryPoint);
        }
    }
    return entryPoints;
}

}  // anonymous namespace

auto collectSources(const std::filesystem::path& projectRoot,
                    const std::vector<Target>& targets) -> std::vector<TargetSources>
{
    const std::vector<std::filesystem::path> scanned = scanSourceDirectory(projectRoot);

    std::vector<TargetSources> collected;
    collected.reserve(targets.size());
    for (std::size_t index = 0; index < targets.size(); ++index) {
        const Target& target = targets[index];
        const std::vector<std::filesystem::path> excluded = otherEntryPoints(targets, index);

        std::vector<std::filesystem::path> sources;
        for (const std::filesystem::path& source : scanned) {
            if (std::ranges::find(excluded, source) == excluded.end()) {
                sources.push_back(source);
            }
        }
        if (std::ranges::find(sources, target.entryPoint) == sources.end()) {
            sources.push_back(target.entryPoint);
            std::ranges::sort(sources);
        }

        collected.push_back(TargetSources{.target = target, .sources = std::move(sources)});
    }
    return collected;
}

}  // namespace scrap::Project
