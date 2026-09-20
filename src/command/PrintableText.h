#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief A path made safe to print.
 *
 * A path carries a directory the user named, which can hold any byte, so
 * control characters and a backslash become \xNN while letters outside ASCII
 * stay as they are. Every message that shows a path passes it through here.
 *
 * @param path Path to render.
 * @return The path as text for a terminal.
 */
[[nodiscard]] auto printablePath(const std::filesystem::path& path) -> std::string;

/**
 * @brief A name made safe to print.
 *
 * A name the user wrote can hold any byte, so printable ASCII stays as it is
 * and every other byte becomes \xNN: control characters and escape sequences
 * reach the terminal as text rather than as instructions. The text is kept
 * whole, since a message that names something has to name it as it is
 * written.
 *
 * @param name Name to render.
 * @return The name as text for a terminal.
 */
[[nodiscard]] auto printableName(std::string_view name) -> std::string;

}  // namespace scrap::Command
