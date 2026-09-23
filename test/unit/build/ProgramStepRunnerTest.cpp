#include "build/BuildStep.h"
#include "build/StepRunner.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <csignal>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using namespace scrap::Build;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * A step that runs @p script in the system shell, in @p directory, and is
 * said to write @p output.
 */
auto shellStep(const std::filesystem::path& directory, const char* output, const std::string& script) -> BuildStep
{
    return BuildStep{ .kind = StepKind::Compile,
                      .target = "hello",
                      .subject = "src/main.cpp",
                      .directory = directory,
                      .output = output,
                      .arguments = { "/bin/sh", "-c", script } };
}

}  // namespace

/**
 * The directory the output goes in is there before the program runs, and the
 * program runs in the step's directory.
 */
TEST(ProgramStepRunnerTest, CreatesTheDirectoryOfTheOutput)
{
    const TempDirectory temp;
    ProgramStepRunner runner;

    const auto result = runner.run(shellStep(temp.path(), "build/obj/a.o", "test -d build/obj && : > build/obj/a.o"));

    EXPECT_FALSE(result.failure.has_value());
    EXPECT_TRUE(std::filesystem::is_regular_file(temp.path() / "build" / "obj" / "a.o"));
}

/**
 * Both output streams come back together, in the order written.
 */
TEST(ProgramStepRunnerTest, ReadsBothStreamsTogether)
{
    const TempDirectory temp;
    ProgramStepRunner runner;

    const auto result = runner.run(shellStep(temp.path(), "a.o", "printf 'a'; printf 'b' >&2; printf 'c'"));

    EXPECT_FALSE(result.failure.has_value());
    EXPECT_EQ(result.output, "abc");
}

/**
 * A program that exits with a status other than 0 fails the step, and what it
 * wrote is kept.
 */
TEST(ProgramStepRunnerTest, ReportsAProgramThatFails)
{
    const TempDirectory temp;
    ProgramStepRunner runner;

    const auto result = runner.run(shellStep(temp.path(), "a.o", "printf 'error' >&2; exit 4"));

    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(result.failure->kind, StepFailureKind::Exited);
    EXPECT_EQ(result.failure->status, 4);
    EXPECT_EQ(result.output, "error");
}

/**
 * A program a signal stops fails the step with that signal.
 */
TEST(ProgramStepRunnerTest, ReportsAProgramASignalStopped)
{
    const TempDirectory temp;
    ProgramStepRunner runner;

    const auto result = runner.run(shellStep(temp.path(), "a.o", "kill -KILL $$"));

    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(result.failure->kind, StepFailureKind::Signalled);
    EXPECT_EQ(result.failure->status, SIGKILL);
}

/**
 * A program that cannot be started fails the step with the program and the
 * system's reason.
 */
TEST(ProgramStepRunnerTest, ReportsAProgramThatCannotStart)
{
    const TempDirectory temp;
    ProgramStepRunner runner;
    BuildStep step = shellStep(temp.path(), "a.o", "");
    step.arguments = { (temp.path() / "absent").string() };

    const auto result = runner.run(step);

    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(result.failure->kind, StepFailureKind::CannotStart);
    EXPECT_EQ(result.failure->path, temp.path() / "absent");
    EXPECT_EQ(result.failure->code, std::errc::no_such_file_or_directory);
}

/**
 * A directory that cannot be created fails the step before its program runs.
 */
TEST(ProgramStepRunnerTest, ReportsADirectoryItCannotCreate)
{
    const TempDirectory temp;
    temp.writeFile("build", "a file where a directory goes");
    ProgramStepRunner runner;

    const auto result = runner.run(shellStep(temp.path(), "build/obj/a.o", ": > ran"));

    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(result.failure->kind, StepFailureKind::CannotCreateDirectory);
    EXPECT_EQ(result.failure->path, temp.path() / "build" / "obj");
    EXPECT_TRUE(result.failure->code);
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "ran"));
}
