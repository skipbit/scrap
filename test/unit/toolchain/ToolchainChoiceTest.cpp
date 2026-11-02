#include "toolchain/ToolchainChoice.h"

#include "toolchain/ToolchainSpecifier.h"

#include <gtest/gtest.h>

#include <variant>

using namespace scrap::Toolchain;

/**
 * The word system names the system's compiler.
 */
TEST(ToolchainChoiceTest, ReadsTheWordSystem)
{
    const auto choice = parseToolchainChoice("system");

    ASSERT_TRUE(choice.has_value()) << choice.error();
    EXPECT_TRUE(std::holds_alternative<SystemToolchain>(*choice));
}

/**
 * Anything else is read as a specifier.
 */
TEST(ToolchainChoiceTest, ReadsASpecifier)
{
    const auto choice = parseToolchainChoice("gcc@15.2.0");

    ASSERT_TRUE(choice.has_value()) << choice.error();
    ASSERT_TRUE(std::holds_alternative<ToolchainSpecifier>(*choice));
    EXPECT_EQ(std::get<ToolchainSpecifier>(*choice).family, "gcc");
}

/**
 * Text that is neither is refused with the reason it is not a specifier;
 * system written otherwise is not the word.
 */
TEST(ToolchainChoiceTest, RefusesTextThatIsNeither)
{
    EXPECT_EQ(parseToolchainChoice("gcc@").error(), "it names no version after '@'");
    EXPECT_EQ(parseToolchainChoice("").error(), "it is empty");
    EXPECT_FALSE(parseToolchainChoice("System").has_value());
}
