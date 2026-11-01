#include "toolchain/GlobalConfig.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

using namespace scrap::Toolchain;

namespace {

/**
 * @p fault as "<line>:<column>: <key>: <message>", leaving out an empty key.
 */
std::string described(const GlobalConfigFault& fault)
{
    std::string text = std::to_string(fault.line) + ":" + std::to_string(fault.column) + ": ";
    if (! fault.key.empty()) {
        text += fault.key + ": ";
    }
    return text + fault.message;
}

/**
 * The default @p text holds, failing the test when it cannot be read.
 */
std::optional<std::string> defaultIn(std::string_view text)
{
    const auto read = readDefaultToolchain(text);
    EXPECT_TRUE(read.has_value()) << described(read.error());
    return read.value_or("<unreadable>");
}

}  // namespace

/**
 * Empty text, and text without the key, hold no default.
 */
TEST(GlobalConfigTest, ReadsNoDefaultWhenNoneIsWritten)
{
    EXPECT_EQ(defaultIn(""), std::nullopt);
    EXPECT_EQ(defaultIn("[defaults]\n"), std::nullopt);
    EXPECT_EQ(defaultIn("[other]\ntoolchain = \"gcc\"\n"), std::nullopt);
}

/**
 * The default is read as written, a specifier or system.
 */
TEST(GlobalConfigTest, ReadsTheDefaultAsWritten)
{
    EXPECT_EQ(defaultIn("[defaults]\ntoolchain = \"gcc@15.2.0\"\n"), "gcc@15.2.0");
    EXPECT_EQ(defaultIn("[defaults]\ntoolchain = \"system\"\n"), "system");
}

/**
 * Keys this version does not know, beside the default or elsewhere, are
 * passed over.
 */
TEST(GlobalConfigTest, PassesOverUnknownKeys)
{
    std::string text = "later = 1\n";
    text += "[defaults]\n";
    text += "toolchain = \"llvm\"\n";
    text += "editor = \"vi\"\n";
    text += "[future]\n";
    text += "key = true\n";

    EXPECT_EQ(defaultIn(text), "llvm");
}

/**
 * Text that cannot be read is refused, naming the line, the column and the
 * key.
 */
TEST(GlobalConfigTest, RefusesTextThatCannotBeRead)
{
    EXPECT_EQ(described(readDefaultToolchain("[defaults\n").error()).substr(0, 6), "1:10: ");
    EXPECT_EQ(described(readDefaultToolchain("defaults = 1\n").error()), "1:12: defaults: must be a table");
    EXPECT_EQ(described(readDefaultToolchain("[defaults]\ntoolchain = 15\n").error()), "2:13: defaults.toolchain: must be a string");
    EXPECT_EQ(described(readDefaultToolchain("[defaults]\ntoolchain = \"gcc@\"\n").error()),
              "2:13: defaults.toolchain: is neither system nor a toolchain specifier: it names no version after '@'");
}

/**
 * Setting the default into empty text, or over another, reads back as set.
 */
TEST(GlobalConfigTest, WritesTheDefault)
{
    const auto first = writeDefaultToolchain("", "gcc@15.1.0");
    ASSERT_TRUE(first.has_value()) << described(first.error());
    EXPECT_EQ(defaultIn(*first), "gcc@15.1.0");

    const auto second = writeDefaultToolchain(*first, "gcc@15.2.0");
    ASSERT_TRUE(second.has_value()) << described(second.error());
    EXPECT_EQ(defaultIn(*second), "gcc@15.2.0");
}

/**
 * Setting or removing the default keeps every other key, so a key a later
 * version wrote survives an earlier one changing the default.
 */
TEST(GlobalConfigTest, KeepsOtherKeysWhenWriting)
{
    const std::string text = "later = 1\n[defaults]\ntoolchain = \"llvm\"\neditor = \"vi\"\n";

    const auto set = writeDefaultToolchain(text, "gcc");
    ASSERT_TRUE(set.has_value()) << described(set.error());
    EXPECT_TRUE(set->contains("later = 1"));
    EXPECT_TRUE(set->contains("editor = 'vi'"));

    const auto removed = writeDefaultToolchain(text, std::nullopt);
    ASSERT_TRUE(removed.has_value()) << described(removed.error());
    EXPECT_EQ(defaultIn(*removed), std::nullopt);
    EXPECT_TRUE(removed->contains("later = 1"));
    EXPECT_TRUE(removed->contains("editor = 'vi'"));
}

/**
 * Removing the only key of the defaults table removes the table, and
 * removing a default that is not set changes nothing.
 */
TEST(GlobalConfigTest, RemovesTheDefault)
{
    EXPECT_EQ(writeDefaultToolchain("[defaults]\ntoolchain = \"gcc\"\n", std::nullopt), std::string{});
    EXPECT_EQ(writeDefaultToolchain("", std::nullopt), std::string{});
}

/**
 * Text that cannot be read is never written over, since what it holds
 * would be lost.
 */
TEST(GlobalConfigTest, RefusesToWriteOverTextThatCannotBeRead)
{
    EXPECT_FALSE(writeDefaultToolchain("[defaults\n", "gcc").has_value());
    EXPECT_EQ(described(writeDefaultToolchain("defaults = 1\n", "gcc").error()), "1:12: defaults: must be a table");
    EXPECT_EQ(described(writeDefaultToolchain("[defaults]\ntoolchain = 15\n", std::nullopt).error()), "2:13: defaults.toolchain: must be a string");
}
