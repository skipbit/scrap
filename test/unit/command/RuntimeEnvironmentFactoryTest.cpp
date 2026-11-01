#include "command/RuntimeEnvironmentFactory.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace scrap::Command;

/**
 * PATH segments are split on ':' and preserved in order.
 */
TEST(RuntimeEnvironmentFactoryTest, SplitsPathInOrder)
{
    auto env = makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .path = "/usr/local/bin:/usr/bin:/bin" });

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
    auto env = makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .path = ":/usr/bin::/bin:" });

    ASSERT_EQ(env.systemSearchPaths.size(), 2);
    EXPECT_EQ(env.systemSearchPaths[0], "/usr/bin");
    EXPECT_EQ(env.systemSearchPaths[1], "/bin");
}

/**
 * A non-empty SCRAP_HOME is carried as it was given.
 */
TEST(RuntimeEnvironmentFactoryTest, CarriesScrapHomeWhenNonEmpty)
{
    auto env = makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .scrapHome = "/opt/scrap", .path = "/usr/bin" });

    EXPECT_EQ(env.scrapHome, std::filesystem::path("/opt/scrap"));
}

/**
 * Without SCRAP_HOME and HOME there is nowhere to keep what scrap installs,
 * rather than a directory named by the empty path.
 */
TEST(RuntimeEnvironmentFactoryTest, LeavesScrapHomeUnsetWithoutScrapHomeOrHome)
{
    auto env = makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .path = "/usr/bin" });

    EXPECT_FALSE(env.scrapHome.has_value());
}

/**
 * Without SCRAP_HOME, or with it empty, scrap keeps what it installs in
 * ".scrap" below HOME.
 */
TEST(RuntimeEnvironmentFactoryTest, FallsBackToDotScrapBelowHome)
{
    auto env = makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .home = "/home/user", .path = "/usr/bin" });

    EXPECT_EQ(env.scrapHome, std::filesystem::path("/home/user/.scrap"));
}

/**
 * SCRAP_HOME, when given, is used in place of ".scrap" below HOME.
 */
TEST(RuntimeEnvironmentFactoryTest, PrefersScrapHomeOverHome)
{
    auto env = makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .scrapHome = "/opt/scrap", .home = "/home/user", .path = "/usr/bin" });

    EXPECT_EQ(env.scrapHome, std::filesystem::path("/opt/scrap"));
}

/**
 * A relative SCRAP_HOME or HOME is taken against the working directory, so
 * scrapHome can be written into a program and still name the same place
 * from anywhere.
 */
TEST(RuntimeEnvironmentFactoryTest, TakesARelativeHomeAgainstTheWorkingDirectory)
{
    EXPECT_EQ(makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .scrapHome = "tools/scrap" }).scrapHome, std::filesystem::path("/project/tools/scrap"));
    EXPECT_EQ(makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .home = "user" }).scrapHome, std::filesystem::path("/project/user/.scrap"));
}

/**
 * Empty SCRAP_HOME, HOME and PATH leave no home and no PATH directory.
 */
TEST(RuntimeEnvironmentFactoryTest, EmptyInputsLeaveNoHomeAndNoPath)
{
    auto env = makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project" });

    EXPECT_FALSE(env.scrapHome.has_value());
    EXPECT_TRUE(env.systemSearchPaths.empty());
}

/**
 * workingDirectory is set directly from the cwd argument.
 */
TEST(RuntimeEnvironmentFactoryTest, WorkingDirectoryMatchesCwd)
{
    const std::filesystem::path cwd = "/some/project/root";
    auto env = makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = cwd });

    EXPECT_EQ(env.workingDirectory, cwd);
}

/**
 * The system's own search paths hold PATH alone: the directory scrap installs
 * into belongs to scrap, so what it holds is never read as the system's.
 */
TEST(RuntimeEnvironmentFactoryTest, SystemSearchPathsHoldPathAlone)
{
    auto env = makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .scrapHome = "/opt/scrap", .path = "/usr/bin:/bin" });

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
    EXPECT_EQ(makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .path = "/usr/bin", .compiler = "clang++" }).preferredCompiler, "clang++");
    EXPECT_EQ(makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .path = "/usr/bin", .compiler = "/opt/gcc/bin/g++" }).preferredCompiler, "/opt/gcc/bin/g++");
}

/**
 * An unset compiler variable leaves the field empty rather than standing for a
 * compiler named by the empty string.
 */
TEST(RuntimeEnvironmentFactoryTest, PreferredCompilerIsEmptyWhenUnset)
{
    EXPECT_TRUE(makeRuntimeEnvironment(ProcessInputs{ .workingDirectory = "/project", .path = "/usr/bin" }).preferredCompiler.empty());
}
