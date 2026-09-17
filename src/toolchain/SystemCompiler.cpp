#include "toolchain/SystemCompiler.h"

#include <array>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace scrap::Toolchain {

namespace {

/// The default C++ compiler a system is expected to provide.
constexpr std::string_view DefaultCompilerName = "c++";

/// Compilers looked for by name once the default does not answer.
constexpr std::array<std::string_view, 2> KnownCompilerNames{"g++", "clang++"};

/**
 * Whether the path names a file this process can run.
 *
 * The permission bits do not carry that on their own: a file only its owner
 * may run is not one another user can, and one whose owner bit is clear can
 * still be reached through its group. The system is asked instead, which is
 * the same question the build will ask when it runs the program.
 */
auto isExecutableFile(const std::filesystem::path& path) -> bool
{
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::status(path, ec);
    if (ec || ! std::filesystem::is_regular_file(status)) {
        return false;
    }
    return ::access(path.c_str(), X_OK) == 0;
}

/**
 * The path made independent of the directory the command ran in, so a later
 * step running the program from elsewhere still names the same file. A path
 * that cannot be resolved is kept as it was found.
 */
auto resolved(const std::filesystem::path& path) -> std::filesystem::path
{
    std::error_code ec;
    std::filesystem::path absolute = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return path;
    }
    return absolute;
}

/**
 * The first of the directories holding an executable of that name.
 */
auto findOnSearchPaths(std::string_view name,
                       const std::vector<std::filesystem::path>& searchPaths) -> std::optional<std::filesystem::path>
{
    for (const std::filesystem::path& directory : searchPaths) {
        std::filesystem::path candidate = directory / name;
        if (isExecutableFile(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

/**
 * What CXX names: a path carrying a directory is read as it stands, since it
 * names one program rather than a program to look for; a bare name is looked
 * for on the search paths.
 */
auto findNamedCompiler(const std::string& preferredCompiler,
                       const std::vector<std::filesystem::path>& searchPaths) -> std::optional<std::filesystem::path>
{
    const std::filesystem::path named{preferredCompiler};
    if (named.has_parent_path()) {
        return isExecutableFile(named) ? std::optional{named} : std::nullopt;
    }
    return findOnSearchPaths(preferredCompiler, searchPaths);
}

}  // anonymous namespace

auto detectSystemCompiler(const std::string& preferredCompiler,
                          const std::vector<std::filesystem::path>& systemSearchPaths)
    -> std::expected<SystemCompiler, NoCompiler>
{
    if (! preferredCompiler.empty()) {
        const auto named = findNamedCompiler(preferredCompiler, systemSearchPaths);
        if (! named.has_value()) {
            return std::unexpected(NoCompiler{.requested = preferredCompiler});
        }
        return SystemCompiler{.path = resolved(*named), .origin = CompilerOrigin::CompilerVariable};
    }

    if (const auto standard = findOnSearchPaths(DefaultCompilerName, systemSearchPaths)) {
        return SystemCompiler{.path = resolved(*standard), .origin = CompilerOrigin::DefaultOnPath};
    }
    for (const std::string_view name : KnownCompilerNames) {
        if (const auto known = findOnSearchPaths(name, systemSearchPaths)) {
            return SystemCompiler{.path = resolved(*known), .origin = CompilerOrigin::KnownName};
        }
    }
    return std::unexpected(NoCompiler{});
}

}  // namespace scrap::Toolchain
