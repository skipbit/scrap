#include "build/StepHistory.h"

#include "build/BuildStep.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <optional>

using namespace scrap::Build;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * A project whose link has already written its executable, from files written
 * an hour before the test starts.
 */
class StepHistoryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        _temp.writeFile("cc", "compiler");
        _temp.writeFile("build/obj/main.o", "object");
        _temp.writeFile("build/bin/app", "executable");
        _temp.writeFile("src/main.cpp", "#include \"hello.h\"\n");
        _temp.writeFile("include/hello.h", "int hello;\n");
        _temp.writeFile("build/obj/main.o.d", "build/obj/main.o: src/main.cpp include/hello.h\n");
        for (const char* file : { "cc", "build/obj/main.o", "build/bin/app", "src/main.cpp", "src", "include/hello.h", "include" }) {
            std::filesystem::last_write_time(_temp.path() / file, std::filesystem::file_time_type::clock::now() - std::chrono::hours{ 1 });
        }
    }

    /**
     * Run @p step through the history as a build would, for a step that
     * succeeds.
     */
    void succeed(const BuildStep& step)
    {
        _history.starting(step);
        _history.succeeded(step);
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

    [[nodiscard]] BuildStep compileStep() const
    {
        return BuildStep{ .kind = StepKind::Compile,
                          .target = "app",
                          .subject = "src/main.cpp",
                          .directory = _temp.path(),
                          .output = "build/obj/main.o",
                          .arguments = { (_temp.path() / "cc").string(), "-c", "src/main.cpp", "-o", "build/obj/main.o" },
                          .inputs = { "src/main.cpp" },
                          .searchedDirectories = { "include" },
                          .dependencyFile = "build/obj/main.o.d" };
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
    succeed(linkStep());

    EXPECT_TRUE(std::filesystem::is_regular_file(recordOfTheLink()));
    EXPECT_TRUE(_history.upToDate(linkStep()));
}

/**
 * A changed input, or a changed command line, needs the step to run.
 */
TEST_F(StepHistoryTest, RunsAStepWhoseInputOrCommandChanged)
{
    succeed(linkStep());

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
    succeed(linkStep());
    std::filesystem::remove(_temp.path() / "build/bin/app");

    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * A record that cannot be read as one needs the step to run.
 */
TEST_F(StepHistoryTest, RunsAStepWhoseRecordIsBroken)
{
    succeed(linkStep());
    _temp.writeFile("build/records/bin/app", "broken");

    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * The record is emptied as the step starts, so a step stopped before it
 * succeeds runs again.
 */
TEST_F(StepHistoryTest, EmptiesTheRecordAsTheStepStarts)
{
    succeed(linkStep());

    _history.starting(linkStep());

    EXPECT_TRUE(std::filesystem::is_regular_file(recordOfTheLink()));
    EXPECT_EQ(std::filesystem::file_size(recordOfTheLink()), 0U);
    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * A file modified after the step started may have changed after the step read
 * it, so the step runs again.
 */
TEST_F(StepHistoryTest, RunsAStepWhoseInputChangedAsItRan)
{
    _history.starting(linkStep());
    const auto started = std::filesystem::last_write_time(recordOfTheLink());
    std::filesystem::last_write_time(_temp.path() / "build/obj/main.o", started + std::chrono::seconds{ 1 });

    _history.succeeded(linkStep());

    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * A file modified as the step started, but not after, leaves the step up to
 * date.
 */
TEST_F(StepHistoryTest, KeepsAStepWhoseInputChangedAsItStarted)
{
    _history.starting(linkStep());
    const auto started = std::filesystem::last_write_time(recordOfTheLink());
    std::filesystem::last_write_time(_temp.path() / "build/obj/main.o", started);

    _history.succeeded(linkStep());

    EXPECT_TRUE(_history.upToDate(linkStep()));
}

/**
 * A step not seen starting keeps no record, since when it started cannot be
 * told.
 */
TEST_F(StepHistoryTest, KeepsNoRecordOfAStepNotSeenStarting)
{
    _history.succeeded(linkStep());

    EXPECT_FALSE(std::filesystem::exists(recordOfTheLink()));
    EXPECT_FALSE(_history.upToDate(linkStep()));
}

/**
 * A step whose output lies outside the build directory keeps no record.
 */
TEST_F(StepHistoryTest, KeepsNoRecordOutsideTheBuildDirectory)
{
    _temp.writeFile("elsewhere/app", "executable");

    succeed(linkStep("elsewhere/app"));

    EXPECT_FALSE(std::filesystem::exists(_temp.path() / "build" / "records"));
    EXPECT_FALSE(_history.upToDate(linkStep("elsewhere/app")));
}

/**
 * A header that appeared while a compilation ran, where it would be found
 * first, changes a directory it watches, so the compilation runs again.
 */
TEST_F(StepHistoryTest, RunsACompilationWhereAHeaderAppearedAsItRan)
{
    succeed(compileStep());
    ASSERT_TRUE(_history.upToDate(compileStep()));

    _history.starting(compileStep());
    const auto started = std::filesystem::last_write_time(_temp.path() / "build/records/obj/main.o");
    std::filesystem::last_write_time(_temp.path() / "src", started + std::chrono::seconds{ 1 });
    _history.succeeded(compileStep());

    EXPECT_FALSE(_history.upToDate(compileStep()));
}
