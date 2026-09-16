#include "project/ProjectLoader.h"

#include "project/ManifestParser.h"
#include "project/ProjectLocator.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>

namespace scrap::Project {

namespace {

/**
 * Resolve @p startDir to the canonical directory the search starts at.
 *
 * The type is read from the unnormalised path so that the operating system
 * resolves every component, including ".." after a missing entry or a file.
 * A path that does not resolve reports not_found whether or not the error
 * code is set, so the type is checked before the code.
 */
auto resolveStart(const std::filesystem::path& startDir) -> std::expected<std::filesystem::path, ProjectError>
{
    std::error_code ec;
    const std::filesystem::path absolute = std::filesystem::absolute(startDir, ec);
    if (ec) {
        return std::unexpected(ProjectError{PathInaccessible{.path = startDir, .reason = ec.message()}});
    }

    const std::filesystem::file_status status = std::filesystem::status(absolute, ec);
    if (status.type() == std::filesystem::file_type::not_found) {
        return std::unexpected(ProjectError{NotADirectory{.path = absolute}});
    }
    if (ec) {
        return std::unexpected(ProjectError{PathInaccessible{.path = absolute, .reason = ec.message()}});
    }
    if (! std::filesystem::is_directory(status)) {
        return std::unexpected(ProjectError{NotADirectory{.path = absolute}});
    }

    std::filesystem::path resolved = std::filesystem::canonical(absolute, ec);
    if (ec) {
        return std::unexpected(ProjectError{PathInaccessible{.path = absolute, .reason = ec.message()}});
    }
    return resolved;
}

}  // anonymous namespace

auto loadProject(const std::filesystem::path& startDir) -> std::expected<LoadedProject, ProjectError>
{
    const auto start = resolveStart(startDir);
    if (! start.has_value()) {
        return std::unexpected(start.error());
    }

    const std::optional<std::filesystem::path> root = findProjectRoot(*start);
    if (! root.has_value()) {
        return std::unexpected(ProjectError{ProjectNotFound{.startDir = *start}});
    }

    auto manifest = loadManifest(*root / ManifestFileName);
    if (! manifest.has_value()) {
        return std::unexpected(ProjectError{std::move(manifest.error())});
    }
    return LoadedProject{.root = *root, .manifest = std::move(*manifest)};
}

}  // namespace scrap::Project
