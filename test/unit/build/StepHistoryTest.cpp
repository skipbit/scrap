#include "build/StepHistory.h"

#include "build/BuildStep.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>

using namespace scrap::Build;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * A project whose link has already written its executable.
 */
class StepHistoryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        _temp.writeFile("cc", "compiler");
        _temp.writeFile("build/obj/main.o", "object");
        _temp.writeFile("build/bin/app", "executable");
    }

    [[nodiscard]] BuildStep linkStep(const std::filesystem::path& output = "build/bin/app") const
    {
        return BuildStep{ .kind = StepKind::Link,
                          .target = "app",
                          .subject = output,
                          .directory = _temp.path(),
                          .output = output,
                          .arguments = { (_temp.path() / "cc").string(), "build/obj/main.o", "-o", output.string() },
                          .inputs = { "build/obj/main.o" },
                          .searchedDirectories = {},
                          .dependencyFile = std::nullopt };
    }

    [[nodiscard]] std::filesystem::path recordOfTheLink() const
    {
        return _temp.path() / "build" / "records" / "bin" / "app";
    }

    TempDirectory _temp;
    RecordedStepHistory _history{ "build" };
};

}  // anonymous namespace

/**
 * A step that has never succeeded is not up to date.
 */
TEST_F(StepHistoryTest, RunsAStepWithoutARecord)
{
    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * Once a step has succeeded, its record lies below records/ at the path of
 * its output, and the step is up to date while nothing it reads changes.
 */
TEST_F(StepHistoryTest, KeepsARecordOfAStepThatSucceeded)
{
    _history.succeeded(linkStep());

    EXPECT_TRUE(std::filesystem::is_regular_file(recordOfTheLink()));
    EXPECT_TRUE(_history.upToDate(linkStep()));
}

/**
 * A changed input, or a changed command line, needs the step to run.
 */
TEST_F(StepHistoryTest, RunsAStepWhoseInputOrCommandChanged)
{
    _history.succeeded(linkStep());

    BuildStep withAnotherFlag = linkStep();
    withAnotherFlag.arguments.emplace_back("-s");
    EXPECT_FALSE(_history.upToDate(withAnotherFlag));

    _temp.writeFile("build/obj/main.o", "another object");
    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * A step whose output is gone runs, whatever its record says.
 */
TEST_F(StepHistoryTest, RunsAStepWhoseOutputIsGone)
{
    _history.succeeded(linkStep());
    std::filesystem::remove(_temp.path() / "build/bin/app");

    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * A record that cannot be read as one needs the step to run.
 */
TEST_F(StepHistoryTest, RunsAStepWhoseRecordIsBroken)
{
    _history.succeeded(linkStep());
    _temp.writeFile("build/records/bin/app", "broken");

    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * The record is forgotten as the step starts, so a step stopped before it
 * succeeds runs again.
 */
TEST_F(StepHistoryTest, ForgetsTheRecordAsTheStepStarts)
{
    _history.succeeded(linkStep());

    _history.starting(linkStep());

    EXPECT_FALSE(std::filesystem::exists(recordOfTheLink()));
    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * A step whose output lies outside the build directory keeps no record.
 */
TEST_F(StepHistoryTest, KeepsNoRecordOutsideTheBuildDirectory)
{
    _temp.writeFile("elsewhere/app", "executable");

    _history.succeeded(linkStep("elsewhere/app"));

    EXPECT_FALSE(std::filesystem::exists(_temp.path() / "build" / "records"));
    EXPECT_FALSE(_history.upToDate(linkStep("elsewhere/app")));
}
