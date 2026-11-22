#include "toolchain/CommandArgument.h"

#include <gtest/gtest.h>

using namespace scrap::Toolchain;

/**
 * A relative path starting with '-' or '@' gains a leading ./, so it is read
 * as a file rather than as an option or a file of options.
 */
TEST(CommandArgumentTest, PrefixesARelativePathThatLooksLikeAnOption)
{
    EXPECT_EQ(pathAsArgument("-x.cpp"), "./-x.cpp");
    EXPECT_EQ(pathAsArgument("@x.cpp"), "./@x.cpp");
}

/**
 * Any other path, and an absolute one, stays as it is.
 */
TEST(CommandArgumentTest, KeepsOtherPaths)
{
    EXPECT_EQ(pathAsArgument("src/a.cpp"), "src/a.cpp");
    EXPECT_EQ(pathAsArgument("/-x.cpp"), "/-x.cpp");
}
