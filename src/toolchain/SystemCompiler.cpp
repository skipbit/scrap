#include "toolchain/SystemCompiler.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Toolchain {

namespace {

/// The default C++ compiler a system is expected to provide.
constexpr std::string_view DefaultCompilerName = "c++";

/// Compilers looked for by name once neither CXX nor the default answers.
constexpr std::array<std::string_view, 2> KnownCompilerNames{"g++", "clang++"};

/**
 * Whether the path names a file that can be run. The check matches the one
 * external commands are found by, so both read the same file the same way.
 */
auto isExecutableFile(const std::filesystem::path& path) -> bool
{
    std::error_code ec;
    if (! std::filesystem::is_regular_file(path, ec) || ec) {
        return false;
    }
    const std::filesystem::file_status status = std::filesystem::status(path, ec);
    if (ec) {
        return false;
    }
    return (status.permissions() & std::filesystem::perms::owner_exec) != std::filesystem::perms::none;
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
    if (preferredCompiler.empty()) {
        return std::nullopt;
    }
    const std::filesystem::path named{preferredCompiler};
    if (named.has_parent_path()) {
        return isExecutableFile(named) ? std::optional{named} : std::nullopt;
    }
    return findOnSearchPaths(preferredCompiler, searchPaths);
}

}  // anonymous namespace

auto detectSystemCompiler(const std::string& preferredCompiler,
                          const std::vector<std::filesystem::path>& systemSearchPaths) -> std::optional<SystemCompiler>
{
    if (const auto named = findNamedCompiler(preferredCompiler, systemSearchPaths)) {
        return SystemCompiler{.path = *named, .origin = CompilerOrigin::CompilerVariable};
    }
    if (const auto standard = findOnSearchPaths(DefaultCompilerName, systemSearchPaths)) {
        return SystemCompiler{.path = *standard, .origin = CompilerOrigin::DefaultOnPath};
    }
    for (const std::string_view name : KnownCompilerNames) {
        if (const auto known = findOnSearchPaths(name, systemSearchPaths)) {
            return SystemCompiler{.path = *known, .origin = CompilerOrigin::KnownName};
        }
    }
    return std::nullopt;
}

}  // namespace scrap::Toolchain
