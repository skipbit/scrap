#include "process/Subprocess.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <csignal>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using namespace scrap::Process;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * A command line that runs @p script in the system shell.
 */
std::vector<std::string> shell(const std::string& script)
{
    return { "/bin/sh", "-c", script };
}

}  // namespace

/**
 * Standard output is read back and standard error is left out.
 */
TEST(SubprocessTest, ReadsStandardOutputAlone)
{
    const auto completion = runProgram(shell("printf out; printf err >&2"), {}, OutputCapture::StandardOutput);

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "out");
    EXPECT_EQ(completion->exitCode, 0);
}

/**
 * Both streams are read together, in the order the program wrote them.
 */
TEST(SubprocessTest, ReadsBothStreamsInTheOrderWritten)
{
    const auto completion = runProgram(shell("printf a; printf b >&2; printf c"), {}, OutputCapture::Combined);

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "abc");
}

/**
 * A program that exits reports its exit code and no signal.
 */
TEST(SubprocessTest, ReportsTheExitCode)
{
    const auto completion = runProgram(shell("exit 3"), {}, OutputCapture::Combined);

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->exitCode, 3);
    EXPECT_FALSE(completion->signal.has_value());
}

/**
 * A program a signal stops reports the signal and no exit code.
 */
TEST(SubprocessTest, ReportsTheSignalThatStoppedIt)
{
    const auto completion = runProgram(shell("kill -KILL $$"), {}, OutputCapture::Combined);

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->signal, SIGKILL);
    EXPECT_FALSE(completion->exitCode.has_value());
}

/**
 * The program runs in the directory given.
 */
TEST(SubprocessTest, RunsInTheDirectoryGiven)
{
    const TempDirectory temp;

    const auto completion = runProgram(shell("pwd -P"), temp.path(), OutputCapture::StandardOutput);

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, std::filesystem::canonical(temp.path()).string() + "\n");
}

/**
 * Each argument reaches the program as written: no shell splits or expands
 * it on the way.
 */
TEST(SubprocessTest, PassesEachArgumentAsWritten)
{
    const std::vector<std::string> arguments{ "/bin/sh", "-c", R"(printf '%s|' "$@")", "sh", "a b", "", "$HOME", "-c" };

    const auto completion = runProgram(arguments, {}, OutputCapture::StandardOutput);

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "a b||$HOME|-c|");
}

/**
 * Nothing is read from the caller's standard input, so a program that reads
 * it finds it empty instead of waiting.
 */
TEST(SubprocessTest, GivesTheProgramNoInput)
{
    const auto completion = runProgram(shell("cat; printf done"), {}, OutputCapture::StandardOutput);

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "done");
}

/**
 * A program that is not there is reported with the system's reason.
 */
TEST(SubprocessTest, ReportsAProgramThatCannotBeStarted)
{
    const TempDirectory temp;
    const std::string absent = (temp.path() / "absent").string();

    const auto completion = runProgram({ absent }, {}, OutputCapture::Combined);

    ASSERT_FALSE(completion.has_value());
    EXPECT_EQ(completion.error(), std::errc::no_such_file_or_directory);
}

/**
 * An empty command line names no program.
 */
TEST(SubprocessTest, RefusesAnEmptyCommandLine)
{
    const auto completion = runProgram({}, {}, OutputCapture::Combined);

    ASSERT_FALSE(completion.has_value());
    EXPECT_EQ(completion.error(), std::errc::invalid_argument);
}
