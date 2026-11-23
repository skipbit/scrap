#include "check/ClangTidy.h"

#include "file/WholeFile.h"
#include "support/TempDirectory.h"
#include "tool/ExternalTool.h"

#include <gtest/gtest.h>

#include <csignal>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using namespace scrap::Check;
using scrap::TestSupport::TempDirectory;
using scrap::Tool::ExternalTool;
using scrap::Tool::ToolFailed;
using scrap::Tool::ToolNotRun;

namespace {

/**
 * A clang-tidy that writes the arguments it was given, one to a line, to the
 * file "arguments" in the directory it runs in, and then runs @p rest.
 */
ExternalTool recordingTool(const TempDirectory& temp, const std::string& rest = "")
{
    const auto program = temp.writeScript("tool/clang-tidy", "printf '%s\\n' \"$@\" > arguments\n" + rest);
    return ExternalTool{ .name = std::string{ ClangTidyName }, .program = program, .version = "18.1.3" };
}

/// The lines of @p file, without their newlines.
std::vector<std::string> linesOf(const std::filesystem::path& file)
{
    const auto text = scrap::File::readWholeFile(file);
    if (! text.has_value()) {
        ADD_FAILURE() << "cannot read " << file;
        return {};
    }
    std::vector<std::string> lines;
    std::string::size_type start = 0;
    while (start < text->size()) {
        const auto end = text->find('\n', start);
        lines.push_back(text->substr(start, end - start));
        start = (end == std::string::npos) ? text->size() : (end + 1);
    }
    return lines;
}

}  // namespace

/**
 * An analysis runs clang-tidy in the project root with the directory of the
 * commands and the file, and returns what it wrote to both streams.
 */
TEST(ClangTidyTest, AnalyzesAFileFromTheProjectRoot)
{
    const TempDirectory temp;
    const ExternalTool tool = recordingTool(temp, "echo 'src/a.cpp:1:1: warning: use nullptr'\necho '1 warning generated.' >&2\n");
    const std::filesystem::path root = temp.makeDirectory("project");

    const auto analyzed = analyzeFile(tool, root, "build/debug", "src/a.cpp", std::nullopt);

    ASSERT_TRUE(analyzed.has_value());
    EXPECT_EQ(*analyzed, "src/a.cpp:1:1: warning: use nullptr\n1 warning generated.\n");
    EXPECT_EQ(linesOf(root / "arguments"), (std::vector<std::string>{ "--quiet", "-p=build/debug", "src/a.cpp" }));
}

/**
 * The checks given take the place of those of .clang-tidy, since --checks
 * alone would add to them.
 */
TEST(ClangTidyTest, PutsTheChecksGivenInPlaceOfThoseOfTheConfiguration)
{
    const TempDirectory temp;
    const ExternalTool tool = recordingTool(temp);
    const std::filesystem::path root = temp.makeDirectory("project");

    ASSERT_TRUE(analyzeFile(tool, root, "build/release", "src/a.cpp", "readability-*,-readability-magic-numbers").has_value());

    EXPECT_EQ(linesOf(root / "arguments"),
              (std::vector<std::string>{ "--quiet", "-p=build/release", "--checks=-*,readability-*,-readability-magic-numbers", "src/a.cpp" }));
}

/**
 * A file whose name starts with '-' or '@' is passed with a leading ./, so
 * clang-tidy does not read it as an option or a file of options.
 */
TEST(ClangTidyTest, PassesAFileThatLooksLikeAnOptionAsAFile)
{
    const TempDirectory temp;
    const ExternalTool tool = recordingTool(temp);
    const std::filesystem::path root = temp.makeDirectory("project");

    ASSERT_TRUE(analyzeFile(tool, root, "build/debug", "-x.cpp", std::nullopt).has_value());
    EXPECT_EQ(linesOf(root / "arguments").back(), "./-x.cpp");

    ASSERT_TRUE(analyzeFile(tool, root, "build/debug", "@x.cpp", std::nullopt).has_value());
    EXPECT_EQ(linesOf(root / "arguments").back(), "./@x.cpp");
}

/**
 * An analysis clang-tidy ends with a status other than 0 comes back with the
 * status and everything it wrote, the diagnostics included.
 */
TEST(ClangTidyTest, ReportsWhatAFailedAnalysisWrote)
{
    const TempDirectory temp;
    const ExternalTool failing = recordingTool(temp, "echo 'src/a.cpp:1:1: error: use nullptr'\necho '1 warning generated.' >&2\nexit 1\n");
    const std::filesystem::path root = temp.makeDirectory("project");

    const auto analyzed = analyzeFile(failing, root, "build/debug", "src/a.cpp", std::nullopt);

    ASSERT_FALSE(analyzed.has_value());
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(analyzed.error()));
    const auto& failure = std::get<ToolFailed>(analyzed.error());
    EXPECT_EQ(failure.exitCode, std::optional<int>{ 1 });
    EXPECT_FALSE(failure.signal.has_value());
    EXPECT_EQ(failure.output, "src/a.cpp:1:1: error: use nullptr\n1 warning generated.\n");
}

/**
 * An analysis stopped by a signal is reported with the signal, and a
 * clang-tidy that cannot be started as such.
 */
TEST(ClangTidyTest, ReportsAnAnalysisStoppedOrNotStarted)
{
    const TempDirectory temp;
    const ExternalTool killed = recordingTool(temp, "kill -KILL $$\n");
    const ExternalTool missing{ .name = std::string{ ClangTidyName }, .program = temp.path() / "missing" / "clang-tidy", .version = "18.1.3" };
    const std::filesystem::path root = temp.makeDirectory("project");

    const auto stopped = analyzeFile(killed, root, "build/debug", "src/a.cpp", std::nullopt);
    const auto notStarted = analyzeFile(missing, root, "build/debug", "src/a.cpp", std::nullopt);

    ASSERT_FALSE(stopped.has_value());
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(stopped.error()));
    EXPECT_EQ(std::get<ToolFailed>(stopped.error()).signal, std::optional<int>{ SIGKILL });
    ASSERT_FALSE(notStarted.has_value());
    EXPECT_TRUE(std::holds_alternative<ToolNotRun>(notStarted.error()));
}
