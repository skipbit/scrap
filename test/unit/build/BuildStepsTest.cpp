#include "build/BuildSteps.h"

#include "build/BuildReporter.h"
#include "build/BuildStep.h"
#include "build/StepRunner.h"
#include "compile/CompileCommand.h"
#include "compile/LinkCommand.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
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
BuildStep stepFor(const char* subject, StepKind kind = StepKind::Compile)
{
    return BuildStep{ .kind = kind,
                      .target = "hello",
                      .subject = subject,
                      .directory = "/home/me/hello",
                      .output = std::string{ subject } + ".o",
                      .arguments = { "/usr/bin/c++", subject } };
}

/**
 * What happened while the build ran, in the order it happened, for the
 * runner and the reporter to write and the steps to wait on.
 */
class Timeline {
public:
    void record(std::string event)
    {
        const std::scoped_lock lock{ _mutex };
        _events.push_back(std::move(event));
        _changed.notify_all();
    }

    /**
     * Wait until @p event has happened. Nothing else ends the wait, so a
     * build that never lets it happen leaves the test to its time limit.
     */
    void waitFor(const std::string& event)
    {
        std::unique_lock lock{ _mutex };
        _changed.wait(lock, [&] {
            return std::ranges::find(_events, event) != _events.end();
        });
    }

    [[nodiscard]] std::vector<std::string> events() const
    {
        const std::scoped_lock lock{ _mutex };
        return _events;
    }

    /**
     * The events that start with @p prefix, in order.
     */
    [[nodiscard]] std::vector<std::string> eventsStartingWith(std::string_view prefix) const
    {
        std::vector<std::string> found;
        for (const std::string& event : events()) {
            if (event.starts_with(prefix)) {
                found.push_back(event);
            }
        }
        return found;
    }

private:
    mutable std::mutex _mutex;
    std::condition_variable _changed;
    std::vector<std::string> _events;
};

/**
 * Carries out a step by doing what it is told the step does; a step it is
 * told nothing of succeeds and writes nothing.
 */
class ScriptedRunner final : public StepRunner {
public:
    explicit ScriptedRunner(Timeline& timeline)
        : _timeline(&timeline)
    {
    }

    std::map<std::filesystem::path, std::function<StepResult()>> scripts;

    StepResult run(const BuildStep& step) override
    {
        _timeline->record("run " + step.subject.string());
        const auto found = scripts.find(step.subject);
        return found == scripts.end() ? StepResult{} : found->second();
    }

private:
    Timeline* _timeline;
};

/**
 * Writes down what it hears on the timeline.
 */
class RecordingReporter final : public BuildReporter {
public:
    explicit RecordingReporter(Timeline& timeline)
        : _timeline(&timeline)
    {
    }

    void started(const BuildStep& step) override
    {
        _timeline->record("started " + step.subject.string());
    }

    void finished(const BuildStep& step, std::string_view output) override
    {
        _timeline->record("finished " + step.subject.string() + " [" + std::string{ output } + "]");
    }

private:
    Timeline* _timeline;
};

/**
 * A result that fails as a compiler that exits with 1, having written
 * @p output.
 */
StepResult failedWith(const char* output)
{
    return StepResult{ .output = output, .failure = StepFailure{ .kind = StepFailureKind::Exited, .path = {}, .code = {}, .status = 1 } };
}

/**
 * The events the reporter wrote, in order.
 */
std::vector<std::string> reported(const Timeline& timeline)
{
    std::vector<std::string> found;
    for (const std::string& event : timeline.events()) {
        if (! event.starts_with("run ")) {
            found.push_back(event);
        }
    }
    return found;
}

}  // namespace

/**
 * Two steps run at once: the first does not end until the second has
 * started, which only a build running them together lets happen.
 */
TEST(BuildStepsTest, RunsStepsAtTheSameTime)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    runner.scripts["src/a.cpp"] = [&] {
        timeline.waitFor("run src/b.cpp");
        return StepResult{};
    };
    RecordingReporter reporter{ timeline };

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, reporter, 2);

    EXPECT_TRUE(result.has_value());
}

/**
 * Each step is reported as it starts, and what it wrote once it has ended,
 * so a step that ends first is heard of first.
 */
TEST(BuildStepsTest, ReportsWhatStepsWroteInTheOrderTheyEnded)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    runner.scripts["src/a.cpp"] = [&] {
        timeline.waitFor("finished src/b.cpp [from b]");
        return StepResult{ .output = "from a", .failure = std::nullopt };
    };
    runner.scripts["src/b.cpp"] = [&] {
        timeline.waitFor("run src/a.cpp");
        return StepResult{ .output = "from b", .failure = std::nullopt };
    };
    RecordingReporter reporter{ timeline };

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, reporter, 2);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(reported(timeline),
              (std::vector<std::string>{ "started src/a.cpp", "started src/b.cpp", "finished src/b.cpp [from b]", "finished src/a.cpp [from a]" }));
}

/**
 * Once a step fails no further step starts, and the step already running is
 * waited for: it fails too, and both come back in the order they ended.
 */
TEST(BuildStepsTest, WaitsForTheRunningStepsAfterAFailure)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    runner.scripts["src/a.cpp"] = [&] {
        timeline.waitFor("run src/b.cpp");
        return failedWith("a: error");
    };
    runner.scripts["src/b.cpp"] = [&] {
        timeline.waitFor("finished src/a.cpp [a: error]");
        return failedWith("b: error");
    };
    RecordingReporter reporter{ timeline };

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp"), stepFor("src/c.cpp"), stepFor("bin/app", StepKind::Link) },
                                 runner,
                                 reporter,
                                 2);

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().size(), 2);
    EXPECT_EQ(result.error()[0].step.subject, "src/a.cpp");
    EXPECT_EQ(result.error()[1].step.subject, "src/b.cpp");
    EXPECT_EQ(result.error()[1].failure.kind, StepFailureKind::Exited);
    EXPECT_EQ(timeline.eventsStartingWith("run "), (std::vector<std::string>{ "run src/a.cpp", "run src/b.cpp" }));
    EXPECT_EQ(reported(timeline).back(), "finished src/b.cpp [b: error]");
}

/**
 * A link waits for every compilation, even with a thread free to run it: the
 * thread that ran the first compilation is free while the second still runs.
 */
TEST(BuildStepsTest, LinksOnceEveryCompilationHasEnded)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    runner.scripts["src/a.cpp"] = [&] {
        timeline.waitFor("run src/b.cpp");
        return StepResult{};
    };
    runner.scripts["src/b.cpp"] = [&] {
        timeline.waitFor("finished src/a.cpp []");
        return StepResult{};
    };
    RecordingReporter reporter{ timeline };

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp"), stepFor("bin/app", StepKind::Link) }, runner, reporter, 2);

    EXPECT_TRUE(result.has_value());
    const auto events = reported(timeline);
    EXPECT_EQ(events.back(), "finished bin/app []");
    EXPECT_LT(std::ranges::find(events, "finished src/b.cpp []"), std::ranges::find(events, "started bin/app"));
}

/**
 * With one step at a time, each step ends before the next starts.
 */
TEST(BuildStepsTest, RunsOneStepAtATimeWhenToldTo)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    runner.scripts["src/b.cpp"] = [] {
        return StepResult{ .output = "warning", .failure = std::nullopt };
    };
    RecordingReporter reporter{ timeline };

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, reporter, 1);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(reported(timeline),
              (std::vector<std::string>{ "started src/a.cpp", "finished src/a.cpp []", "started src/b.cpp", "finished src/b.cpp [warning]" }));
}

/**
 * A build with no steps succeeds without running anything.
 */
TEST(BuildStepsTest, SucceedsWithNothingToRun)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    RecordingReporter reporter{ timeline };

    EXPECT_TRUE(runSteps({}, runner, reporter, 4).has_value());
    EXPECT_TRUE(timeline.events().empty());
}

/**
 * What a step throws reaches the caller, and no further step starts.
 */
TEST(BuildStepsTest, PassesOnWhatAStepThrows)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    runner.scripts["src/a.cpp"] = []() -> StepResult {
        throw std::runtime_error{ "out of memory" };
    };
    RecordingReporter reporter{ timeline };

    EXPECT_THROW((void)runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, reporter, 1), std::runtime_error);
    EXPECT_EQ(timeline.eventsStartingWith("run "), (std::vector<std::string>{ "run src/a.cpp" }));
}

/**
 * Every compilation comes before every link, each named by what it is
 * about: the source it compiles, or the executable it links.
 */
TEST(BuildStepsTest, CompilesBeforeItLinks)
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
