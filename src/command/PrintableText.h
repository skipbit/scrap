#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief Text made safe to print.
 *
 * A value from outside can hold any byte, so control characters, a backslash,
 * the characters that change the direction of text and bytes that form no
 * well-formed UTF-8 sequence become \xNN, while letters outside ASCII stay as
 * they are: what a terminal would act on, or what would show the text in an
 * order other than its bytes, reaches it as text, and what the user typed
 * stays readable.
 *
 * @param text Text to render.
 * @return The text for a terminal.
 */
[[nodiscard]] std::string printableText(std::string_view text);

/**
 * @brief A program's output made safe to print.
 *
 * Escaped as printableText() escapes, except that a line feed, a tab and a
 * backslash stay: the output runs over lines laid out with tabs, and quotes
 * source that holds backslashes.
 *
 * @param text Output to render.
 * @return The output for a terminal.
 */
[[nodiscard]] std::string printableLines(std::string_view text);

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

/// The longest value echoed back, counted before any escape expands it.
inline constexpr std::size_t EchoedValueLimit = 64;

/**
 * @brief A value the user gave, made safe to print and cut where it is
 *        longer than EchoedValueLimit.
 *
 * Every value long enough to be cut is already too long to be a project
 * name, so the cut costs the reader nothing and keeps one screen enough for
 * the message. A target name is not cut: it names something the manifest
 * declares, and a message has to name it as the manifest does.
 *
 * @param text Value to render.
 * @return The value as text for a terminal, ending in "..." when cut.
 */
[[nodiscard]] std::string printableEcho(std::string_view text);

/**
 * @brief Text the user typed, made safe to print as printableText() makes
 *        it and cut where it is longer than EchoedValueLimit.
 *
 * Letters outside ASCII stay readable, so a mistyped letter shows as typed.
 * A cut that splits a letter leaves bytes that printableText() escapes.
 *
 * @param text Text to render.
 * @return The text for a terminal, ending in "..." when cut.
 */
[[nodiscard]] std::string printableTypedEcho(std::string_view text);

}  // namespace scrap::Command
