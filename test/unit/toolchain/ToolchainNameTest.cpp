#include "toolchain/ToolchainName.h"

#include <gtest/gtest.h>

using namespace scrap::Toolchain;

/**
 * The names the published index uses, and others of the same shape, pass.
 */
TEST(ToolchainNameTest, AcceptsLowercaseLettersDigitsAndTheFourMarks)
{
    EXPECT_TRUE(isToolchainName("gcc"));
    EXPECT_TRUE(isToolchainName("g++"));
    EXPECT_TRUE(isToolchainName("x86_64"));
    EXPECT_TRUE(isToolchainName("arm-none-eabi"));
    EXPECT_TRUE(isToolchainName("llvm.next"));
}

/**
 * An uppercase letter is refused, so two names that differ only in case
 * never meet in one directory on a file system that ignores case.
 */
TEST(ToolchainNameTest, RefusesAnUppercaseLetter)
{
    EXPECT_FALSE(isToolchainName("GCC"));
    EXPECT_FALSE(isToolchainName("Linux"));
}

/**
 * A name made of dots alone passes only when it does not name the directory
 * itself or its parent.
 */
TEST(ToolchainNameTest, RefusesTheCurrentAndParentDirectory)
{
    EXPECT_FALSE(isToolchainName("."));
    EXPECT_FALSE(isToolchainName(".."));
    EXPECT_TRUE(isToolchainName("..."));
    EXPECT_TRUE(isToolchainName(".gcc"));
}

TEST(ToolchainNameTest, RefusesAnyOtherCharacter)
{
    EXPECT_FALSE(isToolchainName(""));
    EXPECT_FALSE(isToolchainName("../gcc"));
    EXPECT_FALSE(isToolchainName("gcc\\15"));
    EXPECT_FALSE(isToolchainName("gcc 15"));
    EXPECT_FALSE(isToolchainName("gcc\n"));
    EXPECT_FALSE(isToolchainName("gcc~"));
    EXPECT_FALSE(isToolchainName("gcc\xC3\xA9"));
}
