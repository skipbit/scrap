#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <optional>
#include <ostream>
#include <string_view>

namespace scrap::Toolchain {

/**
 * Show a version by its text when an expectation on it fails.
 */
void PrintTo(const ToolchainVersion& version, std::ostream* out)
{
    *out << version.text();
}

}  // namespace scrap::Toolchain

using namespace scrap::Toolchain;

namespace {

/**
 * The version @p text names, failing the test when it names none.
 */
ToolchainVersion version(std::string_view text)
{
    std::optional<ToolchainVersion> parsed = ToolchainVersion::parse(text);
    if (! parsed.has_value()) {
        ADD_FAILURE() << "'" << text << "' is not a version";
        return *ToolchainVersion::parse("0");
    }
    return *parsed;
}

}  // namespace

/**
 * A version keeps the text it was written as.
 */
TEST(ToolchainVersionTest, KeepsItsText)
{
    EXPECT_EQ(version("15.2.0").text(), "15.2.0");
    EXPECT_EQ(version("015").text(), "015");
}

/**
 * Only digits joined by single dots make a version.
 */
TEST(ToolchainVersionTest, RejectsAnythingButNumbersJoinedByDots)
{
    for (const std::string_view text : { "", "15.x", "v15", "15-2", "15 2", "+15", "-1" }) {
        EXPECT_FALSE(ToolchainVersion::parse(text).has_value()) << "'" << text << "'";
    }
}

/**
 * A dot at either end or two dots in a row leave an empty part, which
 * names no number.
 */
TEST(ToolchainVersionTest, RejectsAnEmptyPart)
{
    for (const std::string_view text : { ".", "15.", ".15", "15..2" }) {
        EXPECT_FALSE(ToolchainVersion::parse(text).has_value()) << "'" << text << "'";
    }
}

/**
 * Parts compare as numbers, so 150 comes after 15 and 9.
 */
TEST(ToolchainVersionTest, ComparesPartsAsNumbers)
{
    EXPECT_LT(version("9.1.0"), version("15.2.0"));
    EXPECT_LT(version("15.2.0"), version("150.1.0"));
    EXPECT_LT(version("15.9.0"), version("15.10.0"));
}

/**
 * A leading zero does not change the number a part names.
 */
TEST(ToolchainVersionTest, ReadsALeadingZeroAsTheSameNumber)
{
    EXPECT_EQ(version("015"), version("15"));
    EXPECT_EQ(version("15.02.0"), version("15.2.0"));
    EXPECT_EQ(version("0.00"), version("0.0"));
}

/**
 * A part longer than any integer type still compares by its value.
 */
TEST(ToolchainVersionTest, ComparesPartsOfAnyLength)
{
    EXPECT_LT(version("99999999999999999999"), version("100000000000000000000"));
    EXPECT_EQ(version("00099999999999999999999"), version("99999999999999999999"));
}

/**
 * A version that is the start of another comes before it, and is not
 * equal to it.
 */
TEST(ToolchainVersionTest, PutsAShorterPrefixFirst)
{
    EXPECT_LT(version("15.2"), version("15.2.0"));
    EXPECT_NE(version("15.2"), version("15.2.0"));
}

/**
 * A prefix matches by whole parts, not by characters.
 */
TEST(ToolchainVersionTest, StartsWithWholeParts)
{
    EXPECT_TRUE(version("15.2.0").startsWith(version("15")));
    EXPECT_TRUE(version("15.2.0").startsWith(version("15.2")));
    EXPECT_TRUE(version("15.2.0").startsWith(version("15.2.0")));
    EXPECT_TRUE(version("15.2.0").startsWith(version("015")));
    EXPECT_FALSE(version("150.1.0").startsWith(version("15")));
    EXPECT_FALSE(version("15.20.0").startsWith(version("15.2")));
    EXPECT_FALSE(version("15.2").startsWith(version("15.2.0")));
}
