#include "command/CheckText.h"

#include "check/ParallelAnalysis.h"
#include "compile/BuildProfile.h"
#include "tool/ExternalTool.h"

#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace scrap::Command;
using scrap::Check::FailedAnalysis;
using scrap::Compile::BuildProfile;
using scrap::Tool::ExternalTool;
using scrap::Tool::ToolFailed;
using scrap::Tool::ToolNotRun;

namespace {

const ExternalTool Tool{ .name = "clang-tidy", .program = "/usr/bin/clang-tidy", .version = "18.1.3" };

/// The analysis of @p file, which the tool ended with status 1.
FailedAnalysis failedWithStatus(const char* file)
{
    return FailedAnalysis{ .file = file, .failure = ToolFailed{ .exitCode = 1, .signal = std::nullopt, .output = "a.cpp:1:1: error: x" } };
}

/// The analysis of @p file, for which the tool could not be started.
FailedAnalysis notRun(const char* file)
{
    return FailedAnalysis{ .file = file, .failure = ToolNotRun{ .code = std::make_error_code(std::errc::permission_denied) } };
}

}  // namespace

/**
 * Each file is named as it starts, and what the tool wrote follows once it
 * has ended, ending in a newline whether or not it did.
 */
TEST(CheckTextTest, ReportsEachFileAndWhatTheToolWrote)
{
    std::ostringstream out;
    StreamAnalysisReporter reporter{ out, false };

    reporter.started("src/a.cpp");
    reporter.finished("src/a.cpp", "src/a.cpp:1:9: warning: use nullptr");
    reporter.started("src/b.cpp");
    reporter.finished("src/b.cpp", "");

    EXPECT_EQ(out.str(), "    Checking src/a.cpp\nsrc/a.cpp:1:9: warning: use nullptr\n    Checking src/b.cpp\n");
}

/**
 * What the tool wrote is escaped, as everything scrap prints from elsewhere
 * is.
 */
TEST(CheckTextTest, EscapesWhatTheToolWrote)
{
    std::ostringstream out;
    StreamAnalysisReporter reporter{ out, false };

    reporter.finished("src/a.cpp", "a\x1b]0;title\x07z\n");

    EXPECT_EQ(out.str().find('\x1b'), std::string::npos) << out.str();
    EXPECT_EQ(out.str().find('\x07'), std::string::npos) << out.str();
}

TEST(CheckTextTest, NamesTheProfileOfACheckThatFinished)
{
    EXPECT_EQ(renderCheckFinished(BuildProfile::Debug), "    Finished debug check\n");
    EXPECT_EQ(renderCheckFinished(BuildProfile::Release), "    Finished release check\n");
}

/**
 * What the tool wrote was shown as each file ended, so the files are named
 * alone, with a signal that stopped the tool, and the hint comes once.
 */
TEST(CheckTextTest, NamesTheFilesThatFailedAndHintsOnce)
{
    const std::vector<FailedAnalysis> failures{
        failedWithStatus("src/a.cpp"),
        FailedAnalysis{ .file = "src/b.cpp", .failure = ToolFailed{ .exitCode = std::nullopt, .signal = 9, .output = "" } },
    };

    EXPECT_EQ(renderAnalysisFailures(Tool, failures),
              "error: failed to check 'src/a.cpp'\n"
              "error: failed to check 'src/b.cpp': clang-tidy was stopped by signal 9\n"
              "hint: fix the errors reported above and run the command again\n");
}

/**
 * A tool that could not be started is named once, though files analyzed at
 * the same time may each have failed to start it.
 */
TEST(CheckTextTest, NamesAToolThatCouldNotBeStartedOnce)
{
    const std::vector<FailedAnalysis> failures{ notRun("src/a.cpp"), notRun("src/b.cpp") };

    const std::string reason = std::make_error_code(std::errc::permission_denied).message();
    EXPECT_EQ(renderAnalysisFailures(Tool, failures), "error: cannot run '/usr/bin/clang-tidy': " + reason + "\n");
}

/**
 * A file that failed alongside a tool that could not be started is still
 * named, with the hint for it.
 */
TEST(CheckTextTest, NamesAFileThatFailedBesideAToolThatCouldNotBeStarted)
{
    const std::vector<FailedAnalysis> failures{ failedWithStatus("src/a.cpp"), notRun("src/b.cpp") };

    const std::string reason = std::make_error_code(std::errc::permission_denied).message();
    EXPECT_EQ(renderAnalysisFailures(Tool, failures),
              "error: failed to check 'src/a.cpp'\n"
              "error: cannot run '/usr/bin/clang-tidy': "
                  + reason
                  + "\n"
                    "hint: fix the errors reported above and run the command again\n");
}
