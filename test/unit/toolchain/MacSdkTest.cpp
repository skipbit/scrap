#include "toolchain/MacSdk.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace scrap::Toolchain;

namespace {

/**
 * A query that tells @p answer and counts how often it was asked.
 */
SdkPathQuery answering(std::optional<std::filesystem::path> answer, int& asked)
{
    return [answer = std::move(answer), &asked]() {
        ++asked;
        return answer;
    };
}

}  // anonymous namespace

TEST(MacSdkTest, PointsTheCompilerAtTheSdkOnDarwin)
{
    int asked = 0;

    const auto arguments = sdkArguments("darwin", "", answering("/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk", asked));

    ASSERT_TRUE(arguments.has_value());
    EXPECT_EQ(*arguments, (std::vector<std::string>{ "-isysroot", "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk" }));
    EXPECT_EQ(asked, 1);
}

/**
 * The compiler reads SDKROOT itself, so the SDK it names is left to it.
 */
TEST(MacSdkTest, LeavesTheSdkTheEnvironmentNamesToTheCompiler)
{
    int asked = 0;

    const auto arguments = sdkArguments("darwin", "/sdk", answering("/other", asked));

    ASSERT_TRUE(arguments.has_value());
    EXPECT_TRUE(arguments->empty());
    EXPECT_EQ(asked, 0);
}

TEST(MacSdkTest, NeedsNoSdkElsewhere)
{
    int asked = 0;

    const auto arguments = sdkArguments("linux", "", answering("/sdk", asked));

    ASSERT_TRUE(arguments.has_value());
    EXPECT_TRUE(arguments->empty());
    EXPECT_EQ(asked, 0);
}

TEST(MacSdkTest, ReportsAnSdkThatCannotBeFound)
{
    int asked = 0;

    EXPECT_FALSE(sdkArguments("darwin", "", answering(std::nullopt, asked)).has_value());
}

#if defined(__APPLE__)
/**
 * xcrun names a directory on a Mac with the Command Line Tools, as every
 * Mac that builds scrap has.
 */
TEST(MacSdkTest, AsksXcrunForTheSdk)
{
    const std::optional<std::filesystem::path> sdk = askXcrunForSdkPath();

    ASSERT_TRUE(sdk.has_value());
    EXPECT_TRUE(std::filesystem::is_directory(*sdk)) << sdk->string();
}
#endif
