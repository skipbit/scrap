#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace scrap::Compile {

/**
 * @brief Read the files a dependency file lists, as GCC and Clang write it
 *        for -MD.
 *
 * The file holds a rule in make's syntax: the object file, a colon, then
 * every file the compilation read, split across lines that end in a
 * backslash. A space, a tab or a '#' in a name is preceded by a backslash
 * and a '$' is doubled; GCC also doubles the backslashes before a space or a
 * tab. A backslash before anything else is kept. The rules -MP adds
 * after it, a header and a colon with nothing after them, are passed over.
 *
 * @param text What the compiler wrote.
 * @return The files after the first colon, in the order written, or nothing
 *         when @p text holds no target, or a second rule with files of its
 *         own.
 */
[[nodiscard]] std::optional<std::vector<std::filesystem::path>> parseDependencyFile(std::string_view text);

}  // namespace scrap::Compile
