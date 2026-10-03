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

/// The default C++ compiler a system is expected to provide.
constexpr std::string_view DefaultCompilerName = "c++";

/// Compilers looked for by name once the default does not answer.
constexpr std::array<std::string_view, 2> KnownCompilerNames{ "g++", "clang++" };

}  // anonymous namespace

std::expected<SystemCompiler, NoCompiler> detectSystemCompiler(const std::string& preferredCompiler,
                                                               const std::vector<std::filesystem::path>& systemSearchPaths)
{
    if (! preferredCompiler.empty()) {
        const auto named = findProgram(preferredCompiler, systemSearchPaths);
        if (! named.has_value()) {
            return std::unexpected(NoCompiler{ .requested = preferredCompiler });
        }
        return SystemCompiler{ .path = absoluteProgramPath(*named), .origin = CompilerOrigin::CompilerVariable };
    }

    if (const auto standard = findOnSearchPaths(DefaultCompilerName, systemSearchPaths)) {
        return SystemCompiler{ .path = absoluteProgramPath(*standard), .origin = CompilerOrigin::DefaultOnPath };
    }
    for (const std::string_view name : KnownCompilerNames) {
        if (const auto known = findOnSearchPaths(name, systemSearchPaths)) {
            return SystemCompiler{ .path = absoluteProgramPath(*known), .origin = CompilerOrigin::KnownName };
        }
    }
    return std::unexpected(NoCompiler{});
}

}  // namespace scrap::Toolchain
