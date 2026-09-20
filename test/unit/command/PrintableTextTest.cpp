#include <gtest/gtest.h>

#include "command/PrintableText.h"

#include <string>
#include <string_view>

using scrap::Command::printableName;
using scrap::Command::printablePath;

/**
 * A byte that starts no well-formed sequence is escaped.
 */
TEST(PrintableTextTest, EscapesBytesThatFormNoUtf8Sequence)
{
    EXPECT_EQ(printablePath("/home/me/\xe4\xbd/hello"), "/home/me/\\xE4\\xBD/hello");
    EXPECT_EQ(printablePath("/home/me/\xff/hello"), "/home/me/\\xFF/hello");
}

/**
 * A name reaches the terminal as text: printable ASCII stays and every other
 * byte is escaped. The text is kept whole, since a message that names
 * something has to name it as it is written.
 */
TEST(PrintableTextTest, EscapesWhatANameCannotPrint)
{
    EXPECT_EQ(printableName("core"), "core");
    EXPECT_EQ(printableName(std::string_view{"a\x1b[31m\x7f\\b"}), "a\\x1B[31m\\x7F\\x5Cb");
    EXPECT_EQ(printableName(std::string(100, 'n')), std::string(100, 'n'));
}
