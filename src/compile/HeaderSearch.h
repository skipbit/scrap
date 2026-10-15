#pragma once

#include "compile/IncludeDirectives.h"

#include <filesystem>
#include <vector>

namespace scrap::Compile {

/**
 * @brief Where the compiler looks for one header, in the order it looks.
 */
struct HeaderSearch {
    std::vector<std::filesystem::path> paths;  ///< The paths looked at.
    bool stopsAtAFile = true;                  ///< Whether a file at one of them ends the search.
};

/**
 * @brief Where the compiler looks for @p included, named by a file in
 *        @p directory, each path once.
 *
 * A quoted name is looked for in @p directory, then below each of
 * @p searched; a bracketed one only below @p searched. A name the file
 * names with include_next is looked for everywhere a quoted one is, since
 * where the file itself was found is not known, and a file found does not
 * end the search. Paths are lexically normalised.
 */
[[nodiscard]] HeaderSearch headerSearch(const IncludedName& included,
                                        const std::filesystem::path& directory,
                                        const std::vector<std::filesystem::path>& searched);

}  // namespace scrap::Compile
