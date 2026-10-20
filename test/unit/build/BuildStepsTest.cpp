#include "build/BuildSteps.h"

#include "build/BuildReporter.h"
#include "build/BuildStep.h"
#include "build/StepHistory.h"
#include "build/StepRunner.h"
#include "compile/ArchiveCommand.h"
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
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace scrap::Build;
using scrap::Compile::ArchiveCommand;
using scrap::Compile::CompileCommand;
using scrap::Compile::LinkCommand;

namespace {

/**
 * The command that compiles @p source for @p target, with what a known
 * compiler is given.
 */
CompileCommand compileCommand(const char* target, const char* source)
{
    const std::string output = std::string{ "build/debug/obj/" } + target + "/" + source + ".o";
    return CompileCommand{ .target = target,
                           .directory = "/home/me/app",
                           .file = source,
                           .output = output,
                           .arguments = { "/usr/bin/c++", "-c", source },
                           .dependencyFile = output + ".d",
                           .searchedDirectories = { "include", "src" } };
}

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
                      .arguments = { "/usr/bin/c++", subject },
                      .inputs = {},
                      .searchedDirectories = {},
                      .dependencyFile = std::nullopt };
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
 * Finds up to date the steps it is told of by subject, and writes down what
 * it hears of the others. While judging a step, does what it is told to.
 */
class ScriptedHistory final : public StepHistory {
public:
    std::set<std::filesystem::path> upToDateSubjects;
    std::map<std::filesystem::path, std::function<void()>> whileJudging;

    bool upToDate(const BuildStep& step) override
    {
        if (const auto found = whileJudging.find(step.subject); found != whileJudging.end()) {
            found->second();
        }
        return upToDateSubjects.contains(step.subject);
    }

    void starting(const BuildStep& step) override
    {
        add("starting " + step.subject.string());
    }

    void succeeded(const BuildStep& step) override
    {
        add("succeeded " + step.subject.string());
    }

    [[nodiscard]] std::vector<std::string> events() const
    {
        const std::scoped_lock lock{ _mutex };
        return _events;
    }

private:
    void add(std::string event)
    {
        const std::scoped_lock lock{ _mutex };
        _events.push_back(std::move(event));
    }

    mutable std::mutex _mutex;
    std::vector<std::string> _events;
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
    ScriptedHistory history;

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, history, reporter, 2);

    EXPECT_TRUE(result.has_value());
}

/**
 * Each step is reported as it starts, and what it wrote once it has ended,
 * so a step that ends first is heard of first. The second step is judged
 * once the first has started, as the order steps start in is not promised.
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
    ScriptedHistory history;
    history.whileJudging["src/b.cpp"] = [&] {
        timeline.waitFor("started src/a.cpp");
    };

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, history, reporter, 2);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(reported(timeline),
              (std::vector<std::string>{ "started src/a.cpp", "started src/b.cpp", "finished src/b.cpp [from b]", "finished src/a.cpp [from a]" }));
}

/**
 * Once a step fails no further step starts, and the step already running is
 * waited for: it fails too, and both come back in the order they ended. The
 * second step is judged once the first runs, as the order steps start in is
 * not promised.
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
    ScriptedHistory history;
    history.whileJudging["src/b.cpp"] = [&] {
        timeline.waitFor("run src/a.cpp");
    };

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp"), stepFor("src/c.cpp"), stepFor("bin/app", StepKind::Link) },
                                 runner,
                                 history,
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
    ScriptedHistory history;

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp"), stepFor("bin/app", StepKind::Link) }, runner, history, reporter, 2);

    EXPECT_TRUE(result.has_value());
    const auto events = reported(timeline);
    EXPECT_EQ(events.back(), "finished bin/app []");
    EXPECT_LT(std::ranges::find(events, "finished src/b.cpp []"), std::ranges::find(events, "started bin/app"));
}

/**
 * An archive waits for every compilation, as a link does.
 */
TEST(BuildStepsTest, ArchivesOnceEveryCompilationHasEnded)
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
    ScriptedHistory history;

    const auto result
        = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp"), stepFor("lib/libcore.a", StepKind::Archive) }, runner, history, reporter, 2);

    EXPECT_TRUE(result.has_value());
    const auto events = reported(timeline);
    EXPECT_LT(std::ranges::find(events, "finished src/b.cpp []"), std::ranges::find(events, "started lib/libcore.a"));
}

/**
 * A link waits for every archive, since it reads the library an archive
 * writes.
 */
TEST(BuildStepsTest, LinksOnceEveryArchiveHasEnded)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    runner.scripts["lib/liba.a"] = [&] {
        timeline.waitFor("run lib/libb.a");
        return StepResult{};
    };
    runner.scripts["lib/libb.a"] = [&] {
        timeline.waitFor("finished lib/liba.a []");
        return StepResult{};
    };
    RecordingReporter reporter{ timeline };
    ScriptedHistory history;

    const auto result = runSteps(
        { stepFor("lib/liba.a", StepKind::Archive), stepFor("lib/libb.a", StepKind::Archive), stepFor("bin/app", StepKind::Link) },
        runner,
        history,
        reporter,
        2);

    EXPECT_TRUE(result.has_value());
    const auto events = reported(timeline);
    EXPECT_LT(std::ranges::find(events, "finished lib/libb.a []"), std::ranges::find(events, "started bin/app"));
}

/**
 * A compilation after a link in the given steps still runs: the link waits
 * only for the compilations before it.
 */
TEST(BuildStepsTest, RunsACompilationThatComesAfterALinkInTheSteps)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    RecordingReporter reporter{ timeline };
    ScriptedHistory history;

    const auto result
        = runSteps({ stepFor("src/a.cpp"), stepFor("bin/app", StepKind::Link), stepFor("src/b.cpp") }, runner, history, reporter, 2);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(timeline.eventsStartingWith("run ").size(), 3);
    const auto events = reported(timeline);
    EXPECT_LT(std::ranges::find(events, "finished src/a.cpp []"), std::ranges::find(events, "started bin/app"));
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
    ScriptedHistory history;

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, history, reporter, 1);

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
    ScriptedHistory history;

    EXPECT_TRUE(runSteps({}, runner, history, reporter, 4).has_value());
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
    ScriptedHistory history;

    EXPECT_THROW((void)runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, history, reporter, 1), std::runtime_error);
    EXPECT_EQ(timeline.eventsStartingWith("run "), (std::vector<std::string>{ "run src/a.cpp" }));
}

/**
 * Every compilation comes before every archive, and every archive before
 * every link, each named by what it is about: the source it compiles, or the
 * file it writes.
 */
TEST(BuildStepsTest, CompilesThenArchivesThenLinks)
{
    const std::vector<CompileCommand> compiles{ compileCommand("app", "src/main.cpp"), compileCommand("tool", "src/tool.cpp") };
    const std::vector<ArchiveCommand> archives{ ArchiveCommand{ .target = "core",
                                                                .directory = "/home/me/app",
                                                                .output = "build/debug/lib/libcore.a",
                                                                .arguments = { "/usr/bin/ar", "rcs", "build/debug/lib/libcore.a", "core.o" },
                                                                .inputs = { "core.o" } } };
    const std::vector<LinkCommand> links{ LinkCommand{ .target = "app",
                                                       .directory = "/home/me/app",
                                                       .output = "build/debug/bin/app",
                                                       .arguments = { "/usr/bin/c++", "main.o", "-o", "build/debug/bin/app" },
                                                       .inputs = { "main.o", "build/debug/lib/libcore.a" } } };

    const auto steps = buildSteps(compiles, archives, links);

    ASSERT_EQ(steps.size(), 4);
    EXPECT_EQ(steps[0].kind, StepKind::Compile);
    EXPECT_EQ(steps[0].subject, "src/main.cpp");
    EXPECT_EQ(steps[0].output, "build/debug/obj/app/src/main.cpp.o");
    EXPECT_EQ(steps[0].inputs, std::vector<std::filesystem::path>{ "src/main.cpp" });
    EXPECT_EQ(steps[0].searchedDirectories, compiles[0].searchedDirectories);
    EXPECT_EQ(steps[0].dependencyFile, compiles[0].dependencyFile);
    EXPECT_EQ(steps[1].target, "tool");
    EXPECT_EQ(steps[2].kind, StepKind::Archive);
    EXPECT_EQ(steps[2].target, "core");
    EXPECT_EQ(steps[2].subject, "build/debug/lib/libcore.a");
    EXPECT_EQ(steps[2].output, "build/debug/lib/libcore.a");
    EXPECT_EQ(steps[2].arguments, archives[0].arguments);
    EXPECT_EQ(steps[2].inputs, archives[0].inputs);
    EXPECT_EQ(steps[3].kind, StepKind::Link);
    EXPECT_EQ(steps[3].target, "app");
    EXPECT_EQ(steps[3].subject, "build/debug/bin/app");
    EXPECT_EQ(steps[3].directory, "/home/me/app");
    EXPECT_EQ(steps[3].arguments, links[0].arguments);
    EXPECT_EQ(steps[3].inputs, links[0].inputs);
}

/**
 * A step the history finds up to date is neither run nor reported, and the
 * history hears only of the others.
 */
TEST(BuildStepsTest, SkipsAStepThatIsUpToDate)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    RecordingReporter reporter{ timeline };
    ScriptedHistory history;
    history.upToDateSubjects = { "src/a.cpp", "bin/app" };

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp"), stepFor("bin/app", StepKind::Link) }, runner, history, reporter, 1);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(timeline.events(), (std::vector<std::string>{ "started src/b.cpp", "run src/b.cpp", "finished src/b.cpp []" }));
    EXPECT_EQ(history.events(), (std::vector<std::string>{ "starting src/b.cpp", "succeeded src/b.cpp" }));
}

/**
 * A step that failed is not remembered as having succeeded.
 */
TEST(BuildStepsTest, RemembersOnlyTheStepsThatSucceeded)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    runner.scripts["src/b.cpp"] = [] {
        return failedWith("error");
    };
    RecordingReporter reporter{ timeline };
    ScriptedHistory history;

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, history, reporter, 1);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(history.events(), (std::vector<std::string>{ "starting src/a.cpp", "succeeded src/a.cpp", "starting src/b.cpp" }));
}

/**
 * A step judged while another fails does not start once that one has
 * failed.
 */
TEST(BuildStepsTest, StartsNoStepJudgedWhileAnotherFailed)
{
    Timeline timeline;
    ScriptedRunner runner{ timeline };
    runner.scripts["src/a.cpp"] = [&] {
        timeline.waitFor("judging src/b.cpp");
        return failedWith("a: error");
    };
    RecordingReporter reporter{ timeline };
    ScriptedHistory history;
    history.whileJudging["src/b.cpp"] = [&] {
        timeline.record("judging src/b.cpp");
        timeline.waitFor("finished src/a.cpp [a: error]");
    };

    const auto result = runSteps({ stepFor("src/a.cpp"), stepFor("src/b.cpp") }, runner, history, reporter, 2);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(timeline.eventsStartingWith("run "), (std::vector<std::string>{ "run src/a.cpp" }));
    EXPECT_EQ(timeline.eventsStartingWith("started "), (std::vector<std::string>{ "started src/a.cpp" }));
}
