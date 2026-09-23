#include "command/PrintableText.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using scrap::Command::printableName;
using scrap::Command::printablePath;
using scrap::Command::printableText;

/**
 * Letters outside ASCII are what the user typed, so a well-formed sequence
 * stays as it is.
 */
TEST(PrintableTextTest, KeepsLettersOutsideAscii)
{
    // U+65E5, three bytes.
    EXPECT_EQ(printableText("\xe6\x97\xa5"), "\xe6\x97\xa5");
}

/**
 * What a terminal acts on is written as text: a control character, an escape
 * sequence, and the backslash that would make the escapes ambiguous.
 */
TEST(PrintableTextTest, EscapesWhatATerminalActsOn)
{
    EXPECT_EQ(printableText(std::string_view{ "a\x1b[31m\x07\x7f\\b" }), "a\\x1B[31m\\x07\\x7F\\x5Cb");
}

/**
 * A terminal in an eight-bit locale acts on 0x80 to 0x9F, so a C1 control
 * character is escaped although its sequence is well formed.
 */
TEST(PrintableTextTest, EscapesAC1ControlCharacter)
{
    // U+0085, the next-line control character.
    EXPECT_EQ(printableText("\xc2\x85"), "\\xC2\\x85");
}

/**
 * A byte that starts no well-formed sequence is escaped, and a path is
 * rendered by the same rule as any other text.
 */
TEST(PrintableTextTest, EscapesBytesThatFormNoUtf8Sequence)
{
    EXPECT_EQ(printablePath("/home/me/\xe4\xbd/hello"), "/home/me/\\xE4\\xBD/hello");
    EXPECT_EQ(printablePath("/home/me/\xff/hello"), "/home/me/\\xFF/hello");
}

/**
 * A name comes from a manifest, where only printable ASCII belongs, so every
 * other byte is escaped. A name is kept whole, since a message naming a
 * target has to name it as the manifest does.
 */
TEST(PrintableTextTest, EscapesWhatANameCannotPrint)
{
    EXPECT_EQ(printableName("core"), "core");
    EXPECT_EQ(printableName(std::string_view{ "a\x1b[31m\x7f\\b" }), "a\\x1B[31m\\x7F\\x5Cb");
    EXPECT_EQ(printableName("\xe6\x97\xa5"), "\\xE6\\x97\\xA5");
    EXPECT_EQ(printableName(std::string(100, 'n')), std::string(100, 'n'));
}
