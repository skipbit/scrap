#pragma once

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
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
    std::filesystem::path path;  ///< The program to run, as an absolute path.
    CompilerOrigin origin = CompilerOrigin::DefaultOnPath;
};

/**
 * @brief Why no compiler was settled on.
 */
struct NoCompiler {
    /**
     * What CXX asked for and could not be used, or empty when it asked for
     * nothing. A request that cannot be met is a different answer from a
     * system that holds no compiler, and is reported as one.
     */
    std::string requested;
};

/**
 * @brief Find the compiler the system provides.
 *
 * What CXX names is used when it can be run. Otherwise the default C++
 * compiler c++ answers, then the compilers known by name, and the first one
 * found is the one used: a system holding several is not something to choose
 * between.
 *
 * CXX is read as the path of one program. A value carrying anything besides
 * that path, a launcher such as "ccache g++" or a flag among them, names no
 * file and is reported: an explicit request is never passed over in favour of
 * another compiler, since a build that quietly used something else would
 * answer a question nobody asked.
 *
 * A path with a directory in it names one program and is read as it stands; a
 * bare name is looked for on the search paths. Whichever way it was found,
 * the answer is absolute, so a later step running it from another directory
 * still names the same program.
 *
 * Whether a file can be run is the system's answer to give, not one the
 * permission bits carry on their own.
 *
 * Only PATH belongs in @p systemSearchPaths. A compiler scrap installed is
 * not one the system provides, and telling the two apart is what makes the
 * answer worth reporting.
 *
 * @param preferredCompiler What CXX asks for, or empty when it says nothing.
 * @param systemSearchPaths Directories PATH lists, in order.
 * @return The compiler to use, or why none was settled on.
 */
[[nodiscard]] auto detectSystemCompiler(const std::string& preferredCompiler,
                                        const std::vector<std::filesystem::path>& systemSearchPaths)
    -> std::expected<SystemCompiler, NoCompiler>;

}  // namespace scrap::Toolchain
