#include "project/ProjectLoader.h"

#include "project/ManifestParser.h"
#include "project/ProjectLocator.h"

#include <expected>  // NOLINT(misc-include-cleaner) — provides std::expected return type
#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>

namespace scrap::Project {

namespace {

/**
 * Make @p path absolute and normal, without a trailing separator, so that
 * "app" and "app/" report the same directory.
 */
auto normalise(const std::filesystem::path& path) -> std::filesystem::path
{
    std::error_code ec;
    std::filesystem::path result = std::filesystem::absolute(path, ec);
    if (ec) {
        result = path;
    }
    result = result.lexically_normal();
    if (! result.has_filename() && result != result.root_path()) {
        result = result.parent_path();
    }
    return result;
}

}  // anonymous namespace

/**
 * Check the start directory, find the project root above it, and read the
 * manifest there.
 */
auto loadProject(const std::filesystem::path& startDir) -> std::expected<LoadedProject, ProjectError>
{
    const std::filesystem::path start = normalise(startDir);

    std::error_code ec;
    if (! std::filesystem::is_directory(start, ec)) {
        return std::unexpected(ProjectError{NotADirectory{.path = start}});
    }

    const std::optional<std::filesystem::path> root = findProjectRoot(start);
    if (! root.has_value()) {
        return std::unexpected(ProjectError{ProjectNotFound{.startDir = start}});
    }

    auto manifest = loadManifest(*root / ManifestFileName);
    if (! manifest.has_value()) {
        return std::unexpected(ProjectError{std::move(manifest.error())});
    }
    return LoadedProject{.root = *root, .manifest = std::move(*manifest)};
}

}  // namespace scrap::Project
