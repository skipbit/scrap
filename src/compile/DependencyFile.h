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
 * The file holds one rule in make's syntax: the object file, a colon, then
 * every file the compilation read, split across lines that end in a
 * backslash. A space or a '#' in a name is preceded by a backslash and a '$'
 * is doubled; a backslash before anything else is kept.
 *
 * @param text What the compiler wrote.
 * @return The files after the colon, in the order written, or nothing when
 *         @p text is not a single rule: no target, or a second one.
 */
[[nodiscard]] std::optional<std::vector<std::filesystem::path>> parseDependencyFile(std::string_view text);

}  // namespace scrap::Compile
