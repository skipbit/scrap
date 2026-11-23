#include "command/ToolOutput.h"

#include <gtest/gtest.h>

#include <string>

using scrap::Command::printableOutput;

namespace {

/**
 * The sequences that colour are kept or removed as asked; text around them
 * is left as it is.
 */
TEST(ToolOutputTest, KeepsOrRemovesTheSequencesThatColor)
{
    EXPECT_EQ(printableOutput("plain text", false), "plain text");
    EXPECT_EQ(printableOutput("\x1b[01;31mred\x1b[0m", false), "red");
    EXPECT_EQ(printableOutput("\x1b[01;31mred\x1b[0m", true), "\x1b[01;31mred\x1b[0m");
    EXPECT_EQ(printableOutput("gcc\x1b[Kclears the line", false), "gccclears the line");
    EXPECT_EQ(printableOutput("gcc\x1b[Kclears the line", true), "gcc\x1b[Kclears the line");
}

/**
 * A diagnostic quotes the source it read, so what it carries is the
 * project's to decide: every other sequence reaches the terminal as text,
 * whether or not colour is kept.
 */
TEST(ToolOutputTest, WritesEveryOtherSequenceAsText)
{
    for (const bool keepColor : { false, true }) {
        // Rewriting the terminal's title, clearing its screen, and a byte
        // that starts neither.
        EXPECT_EQ(printableOutput("\x1b]0;pwned\x07"
                                  "after",
                                  keepColor),
                  "\\x1B]0;pwned\\x07"
                  "after")
            << keepColor;
        EXPECT_EQ(printableOutput("\x1b[2Jgone", keepColor), "\\x1B[2Jgone") << keepColor;
        EXPECT_EQ(printableOutput("a lone \x1b stays", keepColor), "a lone \\x1B stays") << keepColor;
    }
}

/**
 * Only the forms a compiler colours with pass as colour: a sequence that
 * erases more than the rest of the line, or takes a parameter no colour
 * takes, is written as text.
 */
TEST(ToolOutputTest, PassesOnlyTheFormsACompilerColoursWith)
{
    for (const bool keepColor : { false, true }) {
        EXPECT_EQ(printableOutput("a\x1b[2Kb", keepColor), "a\\x1B[2Kb") << keepColor;
        EXPECT_EQ(printableOutput("a\x1b[1Kb", keepColor), "a\\x1B[1Kb") << keepColor;
        EXPECT_EQ(printableOutput("a\x1b[?1mb", keepColor), "a\\x1B[?1mb") << keepColor;
    }
    EXPECT_EQ(printableOutput("\x1b[01;35m\x1b[Ka\x1b[0Kb\x1b[m", true), "\x1b[01;35m\x1b[Ka\x1b[0Kb\x1b[m");
    EXPECT_EQ(printableOutput("\x1b[0;1;31ma\x1b[0m", true), "\x1b[0;1;31ma\x1b[0m");
}

/**
 * A byte that is not an escape can still be one a terminal acts on, so
 * everything printableText() escapes is written as text here too, colour or
 * not. The bytes a diagnostic is laid out with and quotes source in stay.
 */
TEST(ToolOutputTest, WritesEveryOtherControlAsText)
{
    for (const bool keepColor : { false, true }) {
        // CSI and OSC in an eight-bit locale, the bell, a backspace, a
        // carriage return and an overlong encoding.
        EXPECT_EQ(printableOutput("\x9b"
                                  "2J \x9d"
                                  "0;t \x07 \x08 \r \xc0\x9b",
                                  keepColor),
                  "\\x9B2J \\x9D0;t \\x07 \\x08 \\x0D \\xC0\\x9B")
            << keepColor;
        EXPECT_EQ(printableOutput(std::string{ 'a', '\xe2', '\x80', '\xae', 'b' }, keepColor), "a\\xE2\\x80\\xAEb") << keepColor;
        // Lines, a tab, a quoted backslash and the quotes gcc writes in UTF-8.
        EXPECT_EQ(printableOutput("1 |\tprintf(\"a\\n\");\n\xe2\x80\x98x\xe2\x80\x99\n", keepColor), "1 |\tprintf(\"a\\n\");\n\xe2\x80\x98x\xe2\x80\x99\n") << keepColor;
    }
    EXPECT_EQ(printableOutput("\x1b[01;31m\x9b\x1b[0m", true), "\x1b[01;31m\\x9B\x1b[0m");
}

}  // namespace
