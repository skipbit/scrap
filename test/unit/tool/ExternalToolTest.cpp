#include "tool/ExternalTool.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

using namespace scrap::Tool;
using scrap::TestSupport::TempDirectory;

/**
 * A tool is looked for by its name on the search paths in order, and the
 * first that can be run is returned; nothing comes back when none holds it.
 */
TEST(ExternalToolTest, FindsTheFirstToolOnTheSearchPaths)
{
    const TempDirectory temp;
    temp.makeDirectory("empty");
    temp.writeScript("first/clang-format", "exit 0\n");
    temp.writeScript("second/clang-format", "exit 0\n");
    temp.writeScript("second/clang-tidy", "exit 0\n");

    const std::vector<std::filesystem::path> searchPaths{ temp.path() / "empty", temp.path() / "first", temp.path() / "second" };

    EXPECT_EQ(findTool("clang-format", searchPaths), std::optional{ temp.path() / "first" / "clang-format" });
    EXPECT_EQ(findTool("clang-tidy", searchPaths), std::optional{ temp.path() / "second" / "clang-tidy" });
    EXPECT_FALSE(findTool("clang-format", { temp.path() / "empty" }).has_value());
}

/**
 * The version is the word after "version" in the first line --version
 * writes, whoever packaged the program.
 */
TEST(ExternalToolTest, ReadsTheVersionAfterTheWordVersion)
{
    const TempDirectory temp;
    const auto ubuntu = temp.writeScript("ubuntu/clang-format", "[ \"$1\" = --version ] || exit 1\necho 'Ubuntu clang-format version 18.1.3 (1ubuntu1)'\necho 'second line'\n");
    const auto homebrew = temp.writeScript("homebrew/clang-format", "echo 'Homebrew clang-format version 22.1.8'\n");

    const auto fromUbuntu = describeTool(ubuntu);
    const auto fromHomebrew = describeTool(homebrew);

    ASSERT_TRUE(fromUbuntu.has_value());
    EXPECT_EQ(fromUbuntu->program, ubuntu);
    EXPECT_EQ(fromUbuntu->version, "18.1.3");
    ASSERT_TRUE(fromHomebrew.has_value());
    EXPECT_EQ(fromHomebrew->version, "22.1.8");
}

/**
 * A first line without the word "version" is kept whole, so something is
 * still shown of what the program says it is.
 */
TEST(ExternalToolTest, KeepsTheWholeFirstLineWithoutTheWordVersion)
{
    const TempDirectory temp;
    const auto program = temp.writeScript("tool/clang-format", "echo 'a formatter, release 2'\n");

    const auto described = describeTool(program);

    ASSERT_TRUE(described.has_value());
    EXPECT_EQ(described->version, "a formatter, release 2");
}

/**
 * The word "version" is looked for on every line, and without it the first
 * line that is not empty is kept; a program that writes nothing to show has
 * an unknown version.
 */
TEST(ExternalToolTest, FindsTheVersionOnALaterLineOrShowsWhatItCan)
{
    const TempDirectory temp;
    const auto secondLine = temp.writeScript("second/clang-format", "echo 'LLVM (http://llvm.org/):'\necho '  LLVM version 3.8.0'\n");
    const auto blankFirst = temp.writeScript("blank/clang-format", "echo ''\necho 'a formatter'\n");
    const auto silent = temp.writeScript("silent/clang-format", "exit 0\n");

    EXPECT_EQ(describeTool(secondLine)->version, "3.8.0");
    EXPECT_EQ(describeTool(blankFirst)->version, "a formatter");
    EXPECT_EQ(describeTool(silent)->version, "unknown");
}

/**
 * A program named by a relative path is kept as an absolute one, so a
 * command that runs it from the project root still reaches it.
 */
TEST(ExternalToolTest, KeepsAProgramNamedByARelativePathAsAnAbsolutePath)
{
    const TempDirectory temp;
    const auto program = temp.writeScript("tool/clang-format", "echo 'clang-format version 18.1.3'\n");
    const std::filesystem::path relative = program.lexically_relative(std::filesystem::current_path());
    ASSERT_TRUE(relative.is_relative());

    const auto described = describeTool(relative);

    ASSERT_TRUE(described.has_value());
    EXPECT_TRUE(described->program.is_absolute());
    EXPECT_TRUE(std::filesystem::equivalent(described->program, program));
}

/**
 * A program that cannot be started, or that fails to report its version, is
 * reported as such.
 */
TEST(ExternalToolTest, ReportsAVersionItCannotLearn)
{
    const TempDirectory temp;
    const auto failing = temp.writeScript("tool/clang-format", "exit 3\n");

    const auto notStarted = describeTool(temp.path() / "missing" / "clang-format");
    const auto failed = describeTool(failing);

    ASSERT_FALSE(notStarted.has_value());
    EXPECT_TRUE(std::holds_alternative<ToolNotRun>(notStarted.error()));
    ASSERT_FALSE(failed.has_value());
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(failed.error()));
    EXPECT_EQ(std::get<ToolFailed>(failed.error()).exitCode, std::optional<int>{ 3 });
}

/**
 * A --version that fails reports what it wrote to standard error as well.
 */
TEST(ExternalToolTest, ReportsWhyAVersionFailed)
{
    const TempDirectory temp;
    const auto failing = temp.writeScript("tool/clang-format", "echo 'no version for you' >&2\nexit 2\n");

    const auto described = describeTool(failing);

    ASSERT_FALSE(described.has_value());
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(described.error()));
    const auto& failure = std::get<ToolFailed>(described.error());
    EXPECT_EQ(failure.exitCode, std::optional<int>{ 2 });
    EXPECT_EQ(failure.output, "no version for you\n");
}
