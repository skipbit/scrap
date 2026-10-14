#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace scrap::Compile {

/**
 * @brief How the compiler looks for a header a file names.
 */
enum class IncludeKind : std::uint8_t {
    Quoted,  ///< In the directory of the naming file, then the include directories.
    Angled,  ///< In the include directories.
    Next,    ///< In the include directories after the one the naming file was found in.
};

/**
 * @brief A header a file names, by #include or __has_include.
 */
struct IncludedName {
    std::filesystem::path name;  ///< The name as written between the quotes or the brackets.
    IncludeKind kind;            ///< Where the compiler looks for it.

    bool operator==(const IncludedName&) const = default;
};

/**
 * @brief The headers a file names.
 */
struct IncludeDirectives {
    std::vector<IncludedName> names;  ///< Every name read, in the order written.
    bool computed = false;            ///< Whether a name is made by a macro, and so cannot be read.

    bool operator==(const IncludeDirectives&) const = default;
};

/**
 * @brief Read the headers @p text names, as GCC and Clang would look for them.
 *
 * A '#' or "%:" followed by include, import or include_next, and
 * __has_include or __has_include_next followed by a parenthesis, names the
 * header between the quotes or the brackets after it. Blanks and block
 * comments may come between, and lines a backslash continues are joined.
 *
 * Neither comments nor conditions are looked at, so a name in a comment, a
 * string or a branch not taken is read as well: the list may hold more than
 * the compiler includes, never less.
 */
[[nodiscard]] IncludeDirectives parseIncludeDirectives(std::string_view text);

}  // namespace scrap::Compile
