#include "toolchain/SystemCompiler.h"

#include "toolchain/ProgramSearch.h"

#include <array>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Toolchain {

namespace {

/**
 * The names the compiler of one language is looked for by.
 */
struct CompilerNames {
    std::string_view defaultName;                ///< The default compiler a system is expected to provide.
    std::array<std::string_view, 2> knownNames;  ///< Compilers looked for by name once the default does not answer.
};

constexpr CompilerNames CxxCompilerNames{ .defaultName = "c++", .knownNames = { "g++", "clang++" } };

constexpr CompilerNames CCompilerNames{ .defaultName = "cc", .knownNames = { "gcc", "clang" } };

/**
 * The compiler @p names find, as detectSystemCompiler() describes.
 */
std::expected<SystemCompiler, NoCompiler> detect(const CompilerNames& names,
                                                 const std::string& preferredCompiler,
                                                 const std::vector<std::filesystem::path>& systemSearchPaths)
{
    if (! preferredCompiler.empty()) {
        const auto named = findProgram(preferredCompiler, systemSearchPaths);
        if (! named.has_value()) {
            return std::unexpected(NoCompiler{ .requested = preferredCompiler });
        }
        return SystemCompiler{ .path = absoluteProgramPath(*named), .origin = CompilerOrigin::CompilerVariable };
    }

    if (const auto standard = findOnSearchPaths(names.defaultName, systemSearchPaths)) {
        return SystemCompiler{ .path = absoluteProgramPath(*standard), .origin = CompilerOrigin::DefaultOnPath };
    }
    for (const std::string_view name : names.knownNames) {
        if (const auto known = findOnSearchPaths(name, systemSearchPaths)) {
            return SystemCompiler{ .path = absoluteProgramPath(*known), .origin = CompilerOrigin::KnownName };
        }
    }
    return std::unexpected(NoCompiler{});
}

}  // anonymous namespace

std::expected<SystemCompiler, NoCompiler> detectSystemCompiler(const std::string& preferredCompiler,
                                                               const std::vector<std::filesystem::path>& systemSearchPaths)
{
    return detect(CxxCompilerNames, preferredCompiler, systemSearchPaths);
}

std::expected<SystemCompiler, NoCompiler> detectSystemCCompiler(const std::string& preferredCompiler,
                                                                const std::vector<std::filesystem::path>& systemSearchPaths)
{
    return detect(CCompilerNames, preferredCompiler, systemSearchPaths);
}

}  // namespace scrap::Toolchain
