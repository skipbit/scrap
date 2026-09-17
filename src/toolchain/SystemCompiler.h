#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief How the compiler in use was arrived at.
 */
enum class CompilerOrigin : std::uint8_t {
    CompilerVariable,  ///< Named by the CXX environment variable.
    DefaultOnPath,     ///< The default C++ compiler, c++, found on PATH.
    KnownName          ///< A compiler known by name, found on PATH.
};

/**
 * @brief A compiler the system provides.
 */
struct SystemCompiler {
    std::filesystem::path path;  ///< The program to run.
    CompilerOrigin origin = CompilerOrigin::DefaultOnPath;
};

/**
 * @brief Find the compiler the system provides.
 *
 * Looked for in one order: what CXX names, then the default C++ compiler
 * c++, then the compilers known by name. The first one found is the one
 * used, so a system holding several is not something to choose between.
 *
 * CXX naming a path with a directory in it stands on its own; a bare name is
 * looked for on the search paths like any other. A name that leads nowhere
 * moves the search on to the next step rather than ending it.
 *
 * Only PATH belongs in @p systemSearchPaths. A compiler scrap installed is
 * not one the system provides, and telling the two apart is what makes the
 * answer worth reporting.
 *
 * @param preferredCompiler What CXX asks for, or empty when it says nothing.
 * @param systemSearchPaths Directories PATH lists, in order.
 * @return The compiler to use, or nothing when the system provides none.
 */
[[nodiscard]] auto
detectSystemCompiler(const std::string& preferredCompiler,
                     const std::vector<std::filesystem::path>& systemSearchPaths) -> std::optional<SystemCompiler>;

}  // namespace scrap::Toolchain
