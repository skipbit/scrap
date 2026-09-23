#include "build/SerialBuild.h"

#include "build/BuildReporter.h"
#include "build/BuildStep.h"
#include "build/StepRunner.h"
#include "compile/CompileCommand.h"
#include "compile/LinkCommand.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

using namespace scrap::Build;
using scrap::Compile::CompileCommand;
using scrap::Compile::LinkCommand;

namespace {

/**
 * A step named by its subject alone.
 */
BuildStep stepFor(const char* subject)
{
    return BuildStep{ .kind = StepKind::Compile,
                      .target = "hello",
                      .subject = subject,
                      .directory = "/home/me/hello",
                      .output = std::string{ subject } + ".o",
                      .arguments = { "/usr/bin/c++", subject } };
}

/**
 * Carries out a step by looking up what it is told the step comes to.
 */
class ScriptedRunner final : public StepRunner {
public:
    std::map<std::filesystem::path, StepResult> results;
    std::vector<std::filesystem::path> ran;

    StepResult run(const BuildStep& step) override
    {
        ran.push_back(step.subject);
        const auto found = results.find(step.subject);
        return found == results.end() ? StepResult{} : found->second;
    }
};

/**
 * Writes down what it hears, in order.
 */
class RecordingReporter final : public BuildReporter {
public:
    std::vector<std::string> events;

    void started(const BuildStep& step) override
    {
        events.push_back("started " + step.subject.string());
    }

    void finished(const BuildStep& step, std::string_view output) override
    {
        events.push_back("finished " + step.subject.string() + " [" + std::string{ output } + "]");
    }
};

}  // namespace

/**
 * Each step runs in order, and the reporter hears of it before and after.
 */
TEST(SerialBuildTest, RunsEveryStepInOrder)
{
    ScriptedRunner runner;
    runner.results["src/b.cpp"] = StepResult{ .output = "warning", .failure = std::nullopt };
    RecordingReporter reporter;

    const auto result = runSerially({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, reporter);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(
        reporter.events,
        (std::vector<std::string>{ "started src/a.cpp", "finished src/a.cpp []", "started src/b.cpp", "finished src/b.cpp [warning]" }));
}

/**
 * The step that fails is the last one run, and comes back with its failure;
 * what it wrote still reaches the reporter.
 */
TEST(SerialBuildTest, StopsAtTheFirstStepThatFails)
{
    ScriptedRunner runner;
    runner.results["src/b.cpp"]
        = StepResult{ .output = "src/b.cpp:1:1: error",
                      .failure = StepFailure{ .kind = StepFailureKind::Exited, .path = {}, .code = {}, .status = 1 } };
    RecordingReporter reporter;

    const auto result = runSerially({ stepFor("src/a.cpp"), stepFor("src/b.cpp"), stepFor("src/c.cpp") }, runner, reporter);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().step.subject, "src/b.cpp");
    EXPECT_EQ(result.error().failure.kind, StepFailureKind::Exited);
    EXPECT_EQ(result.error().failure.status, 1);
    EXPECT_EQ(runner.ran, (std::vector<std::filesystem::path>{ "src/a.cpp", "src/b.cpp" }));
    EXPECT_EQ(reporter.events.back(), "finished src/b.cpp [src/b.cpp:1:1: error]");
}

/**
 * A build with no steps succeeds without running anything.
 */
TEST(SerialBuildTest, SucceedsWithNothingToRun)
{
    ScriptedRunner runner;
    RecordingReporter reporter;

    EXPECT_TRUE(runSerially({}, runner, reporter).has_value());
    EXPECT_TRUE(reporter.events.empty());
}

/**
 * Every compilation comes before every link, each named by what it is
 * about: the source it compiles, or the executable it links.
 */
TEST(SerialBuildTest, CompilesBeforeItLinks)
{
    const std::vector<CompileCommand> compiles{ CompileCommand{ .target = "app",
                                                                .directory = "/home/me/app",
                                                                .file = "src/main.cpp",
                                                                .output = "build/debug/obj/app/src/main.cpp.o",
                                                                .arguments = { "/usr/bin/c++", "-c", "src/main.cpp" } },
                                                CompileCommand{ .target = "tool",
                                                                .directory = "/home/me/app",
                                                                .file = "src/tool.cpp",
                                                                .output = "build/debug/obj/tool/src/tool.cpp.o",
                                                                .arguments = { "/usr/bin/c++", "-c", "src/tool.cpp" } } };
    const std::vector<LinkCommand> links{ LinkCommand{ .target = "app",
                                                       .directory = "/home/me/app",
                                                       .output = "build/debug/bin/app",
                                                       .arguments = { "/usr/bin/c++", "-o", "build/debug/bin/app" } } };

    const auto steps = buildSteps(compiles, links);

    ASSERT_EQ(steps.size(), 3);
    EXPECT_EQ(steps[0].kind, StepKind::Compile);
    EXPECT_EQ(steps[0].subject, "src/main.cpp");
    EXPECT_EQ(steps[0].output, "build/debug/obj/app/src/main.cpp.o");
    EXPECT_EQ(steps[1].target, "tool");
    EXPECT_EQ(steps[2].kind, StepKind::Link);
    EXPECT_EQ(steps[2].target, "app");
    EXPECT_EQ(steps[2].subject, "build/debug/bin/app");
    EXPECT_EQ(steps[2].directory, "/home/me/app");
    EXPECT_EQ(steps[2].arguments, links[0].arguments);
}
