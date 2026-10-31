#include "toolchain/ToolchainSpecifier.h"

#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using namespace scrap::Toolchain;

namespace {

/**
 * The version @p text names; the tests pass only well-formed ones.
 */
ToolchainVersion version(std::string_view text)
{
    return ToolchainVersion::parse(text).value();
}

/**
 * Why @p text is not a specifier, or an empty string when it is one.
 */
std::string rejectionOf(std::string_view text)
{
    const auto specifier = parseToolchainSpecifier(text);
    return specifier.has_value() ? std::string{} : specifier.error();
}

}  // namespace

/**
 * A family alone names every version of it.
 */
TEST(ToolchainSpecifierTest, ReadsAFamilyAlone)
{
    const auto specifier = parseToolchainSpecifier("gcc");

    ASSERT_TRUE(specifier.has_value()) << specifier.error();
    EXPECT_EQ(specifier->family, "gcc");
    EXPECT_FALSE(specifier->version.has_value());
}

/**
 * The text after @ is the version, and a family may hold a hyphen.
 */
TEST(ToolchainSpecifierTest, ReadsAFamilyAndAVersion)
{
    const auto specifier = parseToolchainSpecifier("arm-gcc@15.2.0");

    ASSERT_TRUE(specifier.has_value()) << specifier.error();
    EXPECT_EQ(specifier->family, "arm-gcc");
    ASSERT_TRUE(specifier->version.has_value());
    EXPECT_EQ(specifier->version->text(), "15.2.0");
}

/**
 * Each malformed form is rejected with its own reason.
 */
TEST(ToolchainSpecifierTest, RejectsMalformedText)
{
    EXPECT_EQ(rejectionOf(""), "it is empty");
    EXPECT_EQ(rejectionOf("@15.2.0"), "it names no family before '@'");
    EXPECT_EQ(rejectionOf("gcc@"), "it names no version after '@'");
    EXPECT_EQ(rejectionOf("gcc@15@2"), "it has more than one '@'");
    EXPECT_EQ(rejectionOf("gcc@15.x"), "its version is not numbers joined by dots");
    EXPECT_EQ(rejectionOf("gcc@15."), "its version is not numbers joined by dots");
    EXPECT_EQ(rejectionOf("gcc@15..2"), "its version is not numbers joined by dots");
}

/**
 * A family alone matches every version of that family and no other.
 */
TEST(ToolchainSpecifierTest, FamilyAloneMatchesEveryVersionOfIt)
{
    const auto specifier = parseToolchainSpecifier("gcc").value();

    EXPECT_TRUE(specifier.matches("gcc", version("15.2.0")));
    EXPECT_TRUE(specifier.matches("gcc", version("150.1.0")));
    EXPECT_FALSE(specifier.matches("llvm", version("15.2.0")));
    EXPECT_FALSE(specifier.matches("gcc-arm", version("15.2.0")));
}

/**
 * A cut-short version matches its series by whole parts.
 */
TEST(ToolchainSpecifierTest, VersionMatchesItsSeries)
{
    const auto specifier = parseToolchainSpecifier("gcc@15").value();

    EXPECT_TRUE(specifier.matches("gcc", version("15.1.0")));
    EXPECT_TRUE(specifier.matches("gcc", version("15.2.0")));
    EXPECT_FALSE(specifier.matches("gcc", version("150.1.0")));
    EXPECT_FALSE(specifier.matches("llvm", version("15.2.0")));
}
