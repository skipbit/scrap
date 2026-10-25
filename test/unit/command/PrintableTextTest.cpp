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
 * A character that changes the direction text is shown in would make a name
 * read in an order other than its bytes, so it is escaped. A string literal
 * holding one misleads whoever reads the test, so each is built from its
 * bytes.
 */
TEST(PrintableTextTest, EscapesWhatChangesTheDirectionOfText)
{
    // U+202A to U+202E, the embeddings, the pop and the overrides.
    EXPECT_EQ(printableText(std::string{ '\xe2', '\x80', '\xaa' }), "\\xE2\\x80\\xAA");
    EXPECT_EQ(printableText(std::string{ '\xe2', '\x80', '\xae' }), "\\xE2\\x80\\xAE");
    // U+2066 to U+2069, the isolates and their pop.
    EXPECT_EQ(printableText(std::string{ '\xe2', '\x81', '\xa6' }), "\\xE2\\x81\\xA6");
    EXPECT_EQ(printableText(std::string{ '\xe2', '\x81', '\xa9' }), "\\xE2\\x81\\xA9");
    // U+202F and U+2070, just after each range, stay.
    EXPECT_EQ(printableText("\xe2\x80\xaf"), "\xe2\x80\xaf");
    EXPECT_EQ(printableText("\xe2\x81\xb0"), "\xe2\x81\xb0");
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
 * A sequence whose bits look right is still not well formed when it encodes
 * a character in more bytes than it needs, a surrogate, or a value above
 * U+10FFFF, so each of its bytes is escaped.
 */
TEST(PrintableTextTest, EscapesSequencesUnicodeDoesNotAllow)
{
    EXPECT_EQ(printableText("\xc0\x9b"), "\\xC0\\x9B");
    EXPECT_EQ(printableText("\xe0\x82\x9b"), "\\xE0\\x82\\x9B");
    EXPECT_EQ(printableText("\xed\xa0\x80"), "\\xED\\xA0\\x80");
    EXPECT_EQ(printableText("\xf4\x90\x80\x80"), "\\xF4\\x90\\x80\\x80");
    EXPECT_EQ(printableText("\xf5\x80\x80\x80"), "\\xF5\\x80\\x80\\x80");
}

/**
 * The characters next to what Unicode does not allow stay as they are.
 */
TEST(PrintableTextTest, KeepsTheCharactersAtTheEdgesOfWhatUnicodeAllows)
{
    // U+00A0, U+0800, U+D7FF, U+E000, U+10000 and U+10FFFF.
    for (const std::string_view text : { "\xc2\xa0", "\xe0\xa0\x80", "\xed\x9f\xbf", "\xee\x80\x80", "\xf0\x90\x80\x80", "\xf4\x8f\xbf\xbf" }) {
        EXPECT_EQ(printableText(text), text);
    }
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
