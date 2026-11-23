#include "check/ParallelAnalysis.h"

#include "tool/ExternalTool.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

using namespace scrap::Check;
using scrap::Tool::ToolFailed;
using scrap::Tool::ToolNotRun;

namespace {

/**
 * What happened while the files were analyzed, in the order it happened, for
 * the analyzer and the reporter to write and the analyses to wait on.
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
     * Wait until @p event has happened. Nothing else ends the wait, so an
     * analysis that never lets it happen leaves the test to its time limit.
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
     * The events that start with one of @p prefixes, in order.
     */
    [[nodiscard]] std::vector<std::string> eventsStartingWith(std::initializer_list<std::string_view> prefixes) const
    {
        std::vector<std::string> found;
        std::ranges::copy_if(events(), std::back_inserter(found), [prefixes](const std::string& event) {
            return std::ranges::any_of(prefixes, [&event](const std::string_view prefix) {
                return event.starts_with(prefix);
            });
        });
        return found;
    }

private:
    mutable std::mutex _mutex;
    std::condition_variable _changed;
    std::vector<std::string> _events;
};

/**
 * Analyzes a file by doing what it is told the file comes to; a file it is
 * told nothing of succeeds and writes nothing. Each analysis is recorded as
 * "run <file>" before it does so.
 */
class ScriptedAnalyzer {
public:
    explicit ScriptedAnalyzer(Timeline& timeline)
        : _timeline(&timeline)
    {
    }

    std::map<std::string, std::function<FileResult()>> scripts;

    /// The analyzer to hand to analyzeFiles.
    [[nodiscard]] FileAnalyzer analyzer()
    {
        return [this](const std::filesystem::path& file) -> FileResult {
            _timeline->record("run " + file.string());
            const auto script = scripts.find(file.string());
            return (script == scripts.end()) ? FileResult{ "" } : script->second();
        };
    }

private:
    Timeline* _timeline;
};

/**
 * Records each call as "started <file>" or "finished <file> [<output>]", and
 * fails the test when two calls overlap.
 */
class RecordingReporter final : public AnalysisReporter {
public:
    explicit RecordingReporter(Timeline& timeline)
        : _timeline(&timeline)
    {
    }

    void started(const std::filesystem::path& file) override
    {
        enter();
        _timeline->record("started " + file.string());
        leave();
    }

    void finished(const std::filesystem::path& file, const std::string_view output) override
    {
        enter();
        _timeline->record("finished " + file.string() + " [" + std::string{ output } + "]");
        leave();
    }

private:
    void enter()
    {
        const std::scoped_lock lock{ _mutex };
        if (_inside) {
            ADD_FAILURE() << "the reporter was called from two threads together";
        }
        _inside = true;
    }

    void leave()
    {
        const std::scoped_lock lock{ _mutex };
        _inside = false;
    }

    Timeline* _timeline;
    std::mutex _mutex;
    bool _inside = false;
};

/// The calls the reporter heard, in order.
std::vector<std::string> reported(const Timeline& timeline)
{
    return timeline.eventsStartingWith({ "started ", "finished " });
}

/// An analysis the tool ended with status 1, having written @p output.
FileResult failedWith(const std::string& output)
{
    return std::unexpected(ToolFailed{ .exitCode = 1, .signal = std::nullopt, .output = output });
}

/// An analysis whose tool could not be started.
FileResult notRun()
{
    return std::unexpected(ToolNotRun{ .code = std::make_error_code(std::errc::no_such_file_or_directory) });
}

}  // namespace

/**
 * Every file is analyzed whatever the others came to, and only those whose
 * analysis failed come back, with why.
 */
TEST(ParallelAnalysisTest, AnalyzesEveryFileWhateverTheOthersCameTo)
{
    Timeline timeline;
    ScriptedAnalyzer analyzer{ timeline };
    analyzer.scripts["src/a.cpp"] = [] {
        return failedWith("a: error");
    };
    analyzer.scripts["src/b.cpp"] = [] {
        return FileResult{ "b: warning" };
    };
    RecordingReporter reporter{ timeline };

    const auto failures = analyzeFiles({ "src/a.cpp", "src/b.cpp", "src/c.cpp" }, analyzer.analyzer(), reporter, 1);

    ASSERT_EQ(failures.size(), 1);
    EXPECT_EQ(failures[0].file, "src/a.cpp");
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(failures[0].failure));
    EXPECT_EQ(std::get<ToolFailed>(failures[0].failure).exitCode, std::optional<int>{ 1 });
    EXPECT_EQ(reported(timeline),
              (std::vector<std::string>{ "started src/a.cpp",
                                         "finished src/a.cpp [a: error]",
                                         "started src/b.cpp",
                                         "finished src/b.cpp [b: warning]",
                                         "started src/c.cpp",
                                         "finished src/c.cpp []" }));
}

/**
 * Files are analyzed at the same time, as many as allowed: the first waits
 * for the second to run.
 */
TEST(ParallelAnalysisTest, AnalyzesFilesAtTheSameTime)
{
    Timeline timeline;
    ScriptedAnalyzer analyzer{ timeline };
    analyzer.scripts["src/a.cpp"] = [&] {
        timeline.waitFor("run src/b.cpp");
        return FileResult{ "" };
    };
    RecordingReporter reporter{ timeline };

    EXPECT_TRUE(analyzeFiles({ "src/a.cpp", "src/b.cpp" }, analyzer.analyzer(), reporter, 2).empty());
}

/**
 * Each file is reported as it starts, and what the tool wrote once it has
 * ended, so a file that ends first is heard of first; failures come back in
 * that order too.
 */
TEST(ParallelAnalysisTest, ReportsWhatFilesWroteInTheOrderTheyEnded)
{
    Timeline timeline;
    ScriptedAnalyzer analyzer{ timeline };
    analyzer.scripts["src/a.cpp"] = [&] {
        timeline.waitFor("finished src/b.cpp [from b]");
        return failedWith("from a");
    };
    analyzer.scripts["src/b.cpp"] = [&] {
        timeline.waitFor("run src/a.cpp");
        return failedWith("from b");
    };
    RecordingReporter reporter{ timeline };

    const auto failures = analyzeFiles({ "src/a.cpp", "src/b.cpp" }, analyzer.analyzer(), reporter, 2);

    ASSERT_EQ(failures.size(), 2);
    EXPECT_EQ(failures[0].file, "src/b.cpp");
    EXPECT_EQ(failures[1].file, "src/a.cpp");
    EXPECT_EQ(reported(timeline).back(), "finished src/a.cpp [from a]");
}

/**
 * A tool that cannot be started stops the analysis: no further file
 * starts, and the file already being analyzed is waited for.
 */
TEST(ParallelAnalysisTest, StopsOnceTheToolCannotBeStarted)
{
    Timeline timeline;
    ScriptedAnalyzer analyzer{ timeline };
    analyzer.scripts["src/a.cpp"] = [&] {
        timeline.waitFor("finished src/b.cpp []");
        return FileResult{ "a: warning" };
    };
    analyzer.scripts["src/b.cpp"] = [&] {
        timeline.waitFor("run src/a.cpp");
        return notRun();
    };
    RecordingReporter reporter{ timeline };

    const auto failures = analyzeFiles({ "src/a.cpp", "src/b.cpp", "src/c.cpp", "src/d.cpp" }, analyzer.analyzer(), reporter, 2);

    ASSERT_EQ(failures.size(), 1);
    EXPECT_EQ(failures[0].file, "src/b.cpp");
    EXPECT_TRUE(std::holds_alternative<ToolNotRun>(failures[0].failure));
    // The two files start together, in either order.
    std::vector<std::string> runs = timeline.eventsStartingWith({ "run " });
    std::ranges::sort(runs);
    EXPECT_EQ(runs, (std::vector<std::string>{ "run src/a.cpp", "run src/b.cpp" }));
    EXPECT_EQ(reported(timeline).back(), "finished src/a.cpp [a: warning]");
}

/**
 * Nothing to analyze succeeds without telling the reporter anything.
 */
TEST(ParallelAnalysisTest, SucceedsWithNothingToAnalyze)
{
    Timeline timeline;
    ScriptedAnalyzer analyzer{ timeline };
    RecordingReporter reporter{ timeline };

    EXPECT_TRUE(analyzeFiles({}, analyzer.analyzer(), reporter, 4).empty());
    EXPECT_TRUE(timeline.events().empty());
}

/**
 * What an analysis throws reaches the caller, and no further file starts.
 */
TEST(ParallelAnalysisTest, PassesOnWhatAnAnalysisThrows)
{
    Timeline timeline;
    ScriptedAnalyzer analyzer{ timeline };
    analyzer.scripts["src/a.cpp"] = []() -> FileResult {
        throw std::runtime_error{ "out of memory" };
    };
    RecordingReporter reporter{ timeline };

    EXPECT_THROW((void)analyzeFiles({ "src/a.cpp", "src/b.cpp" }, analyzer.analyzer(), reporter, 1), std::runtime_error);
    EXPECT_EQ(timeline.eventsStartingWith({ "run " }), (std::vector<std::string>{ "run src/a.cpp" }));
}
