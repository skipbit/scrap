#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief Text made safe to print.
 *
 * A value from outside can hold any byte, so control characters, a backslash
 * and bytes that form no well-formed UTF-8 sequence become \xNN, while letters
 * outside ASCII stay as they are: what a terminal would act on reaches it as
 * text, and what the user typed stays readable.
 *
 * @param text Text to render.
 * @return The text for a terminal.
 */
[[nodiscard]] std::string printableText(std::string_view text);

/**
 * @brief A path made safe to print.
 *
 * A path carries a directory the user named, which can hold any byte. Every
 * message that shows a path passes it through here.
 *
 * @param path Path to render.
 * @return The path as text for a terminal.
 */
[[nodiscard]] std::string printablePath(const std::filesystem::path& path);

/**
 * @brief A name made safe to print.
 *
 * A name comes from a manifest or from the rule a project name follows, where
 * only printable ASCII belongs, so every other byte becomes \xNN. Text the
 * user typed keeps its letters instead, and printableText() is the one for
 * that. The name is kept whole, since a message naming a target has to name
 * it as the manifest does.
 *
 * @param name Name to render.
 * @return The name as text for a terminal.
 */
[[nodiscard]] std::string printableName(std::string_view name);

}  // namespace scrap::Command
