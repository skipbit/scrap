#pragma once

#include <iosfwd>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief @p text as it can be written to a terminal.
 *
 * The sequences a compiler colours its diagnostics with are kept when
 * @p keepColor says to, and removed when it does not. Everything else is
 * escaped as printableLines() escapes, every other escape sequence included:
 * a diagnostic quotes the source it read, so a project being built decides
 * what it holds, and a terminal is not asked to act on it.
 */
[[nodiscard]] std::string printableOutput(std::string_view text, bool keepColor);

/**
 * @brief Whether standard error goes to a terminal.
 */
[[nodiscard]] bool standardErrorIsTerminal();

/**
 * @brief Write what a tool wrote, as printableOutput() prints it, ending in
 *        a newline.
 *
 * Nothing is written for empty @p output.
 *
 * @param out Where the output goes.
 * @param output What the tool wrote.
 * @param keepColor Whether to leave the colour the tool wrote.
 */
void writeToolOutput(std::ostream& out, std::string_view output, bool keepColor);

}  // namespace scrap::Command
