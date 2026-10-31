#include "command/RuntimeEnvironmentFactory.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace scrap::Command;

/**
 * PATH segments are split on ':' and preserved in order.
 */
TEST(RuntimeEnvironmentFactoryTest, SplitsPathIntoSearchPathsInOrder)
{
    auto env = makeRuntimeEnvironment("/project", "", "/usr/local/bin:/usr/bin:/bin", "");

    ASSERT_EQ(env.systemSearchPaths.size(), 3);
    EXPECT_EQ(env.systemSearchPaths[0], "/usr/local/bin");
    EXPECT_EQ(env.systemSearchPaths[1], "/usr/bin");
    EXPECT_EQ(env.systemSearchPaths[2], "/bin");
}

/**
 * Empty PATH segments (doubled, leading, or trailing colons) are skipped.
 */
TEST(RuntimeEnvironmentFactoryTest, SkipsEmptyPathSegments)
{
    auto env = makeRuntimeEnvironment("/project", "", ":/usr/bin::/bin:", "");

    ASSERT_EQ(env.systemSearchPaths.size(), 2);
    EXPECT_EQ(env.systemSearchPaths[0], "/usr/bin");
    EXPECT_EQ(env.systemSearchPaths[1], "/bin");
}

/**
 * A non-empty SCRAP_HOME is carried as it was given.
 */
TEST(RuntimeEnvironmentFactoryTest, CarriesScrapHomeWhenNonEmpty)
{
    auto env = makeRuntimeEnvironment("/project", "/opt/scrap", "/usr/bin", "");

    EXPECT_EQ(env.scrapHome, std::filesystem::path("/opt/scrap"));
}

/**
 * An empty SCRAP_HOME leaves scrapHome unset rather than naming the empty path.
 */
TEST(RuntimeEnvironmentFactoryTest, LeavesScrapHomeUnsetWhenEmpty)
{
    auto env = makeRuntimeEnvironment("/project", "", "/usr/bin", "");

    EXPECT_FALSE(env.scrapHome.has_value());
}

/**
 * Empty SCRAP_HOME and empty PATH yield no search paths at all.
 */
TEST(RuntimeEnvironmentFactoryTest, EmptyInputsYieldEmptySearchPaths)
{
    auto env = makeRuntimeEnvironment("/project", "", "", "");

    EXPECT_FALSE(env.scrapHome.has_value());
    EXPECT_TRUE(env.systemSearchPaths.empty());
}

/**
 * workingDirectory is set directly from the cwd argument.
 */
TEST(RuntimeEnvironmentFactoryTest, WorkingDirectoryMatchesCwd)
{
    const std::filesystem::path cwd = "/some/project/root";
    auto env = makeRuntimeEnvironment(cwd, "", "", "");

    EXPECT_EQ(env.workingDirectory, cwd);
}

/**
 * The system's own search paths hold PATH alone: the directory scrap installs
 * into belongs to scrap, so what it holds is never read as the system's.
 */
TEST(RuntimeEnvironmentFactoryTest, SystemSearchPathsHoldPathAlone)
{
    auto env = makeRuntimeEnvironment("/project", "/opt/scrap", "/usr/bin:/bin", "");

    ASSERT_EQ(env.systemSearchPaths.size(), 2);
    EXPECT_EQ(env.systemSearchPaths[0], "/usr/bin");
    EXPECT_EQ(env.systemSearchPaths[1], "/bin");
    EXPECT_EQ(env.scrapHome, std::filesystem::path("/opt/scrap"));
}

/**
 * The compiler the environment asks for is carried as it was given.
 */
TEST(RuntimeEnvironmentFactoryTest, PreferredCompilerCarriesTheCompilerVariable)
{
    EXPECT_EQ(makeRuntimeEnvironment("/project", "", "/usr/bin", "clang++").preferredCompiler, "clang++");
    EXPECT_EQ(makeRuntimeEnvironment("/project", "", "/usr/bin", "/opt/gcc/bin/g++").preferredCompiler, "/opt/gcc/bin/g++");
}

/**
 * An unset compiler variable leaves the field empty rather than standing for a
 * compiler named by the empty string.
 */
TEST(RuntimeEnvironmentFactoryTest, PreferredCompilerIsEmptyWhenUnset)
{
    EXPECT_TRUE(makeRuntimeEnvironment("/project", "", "/usr/bin", "").preferredCompiler.empty());
}
