#include "command/FormatText.h"

#include "format/ClangFormat.h"
#include "tool/ExternalTool.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <system_error>

using namespace scrap::Command;
using scrap::Tool::ExternalTool;
using scrap::Tool::ToolFailed;
using scrap::Tool::ToolNotRun;

namespace {

const ExternalTool Tool{ .name = "clang-format", .program = "/usr/bin/clang-format", .version = "18.1.3" };

}  // namespace

/**
 * The file comes first, and what clang-format wrote below it, ending in a
 * newline whether or not it did.
 */
TEST(FormatTextTest, RendersAFormatThatFailedWithWhatClangFormatWrote)
{
    const ToolFailed failed{ .exitCode = 1, .signal = std::nullopt, .output = "Invalid value for -style" };

    EXPECT_EQ(renderFormatFailure(Tool, "src/a.cpp", failed), "error: failed to format 'src/a.cpp'\nInvalid value for -style\n");
}

TEST(FormatTextTest, RendersAFormatASignalStopped)
{
    const ToolFailed failed{ .exitCode = std::nullopt, .signal = 9, .output = "" };

    EXPECT_EQ(renderFormatFailure(Tool, "src/a.cpp", failed), "error: failed to format 'src/a.cpp': clang-format was stopped by signal 9\n");
}

TEST(FormatTextTest, RendersAClangFormatThatCouldNotBeStarted)
{
    const ToolNotRun notRun{ .code = std::make_error_code(std::errc::permission_denied) };

    EXPECT_EQ(renderFormatFailure(Tool, "src/a.cpp", notRun), "error: cannot run '/usr/bin/clang-format': " + notRun.code.message() + "\n");
}

/**
 * What clang-format wrote is escaped, as everything scrap prints from
 * elsewhere is.
 */
TEST(FormatTextTest, EscapesWhatClangFormatWrote)
{
    const ToolFailed failed{ .exitCode = 1, .signal = std::nullopt, .output = "a\x1b]0;title\x07z\n" };

    const std::string text = renderFormatFailure(Tool, "src/a.cpp", failed);

    EXPECT_EQ(text.find('\x1b'), std::string::npos) << text;
    EXPECT_EQ(text.find('\x07'), std::string::npos) << text;
}

/**
 * A clang-format that failed and wrote nothing is reported with its exit
 * status, which is then all there is to say why.
 */
TEST(FormatTextTest, RendersTheExitStatusOfAFormatThatFailedWithoutAWord)
{
    const ToolFailed failed{ .exitCode = 3, .signal = std::nullopt, .output = "" };

    EXPECT_EQ(renderFormatFailure(Tool, "src/a.cpp", failed), "error: failed to format 'src/a.cpp': clang-format exited with status 3\n");
}

TEST(FormatTextTest, RendersAFileACheckFoundUnformatted)
{
    EXPECT_EQ(renderNotFormatted("src/a.cpp"), "warning: 'src/a.cpp' is not formatted\n");
    EXPECT_EQ(renderFormatHint(), "hint: run the same command without --check to format the files\n");
}

/**
 * A check that clang-format failed reads as a format that did, and one that
 * could not read the file back says so.
 */
TEST(FormatTextTest, RendersACheckThatFailed)
{
    const ToolFailed failed{ .exitCode = 1, .signal = std::nullopt, .output = "Invalid value for -style\n" };

    const ToolNotRun notRun{ .code = std::make_error_code(std::errc::permission_denied) };

    EXPECT_EQ(renderCheckFailure(Tool, "src/a.cpp", failed), "error: failed to check 'src/a.cpp'\nInvalid value for -style\n");
    EXPECT_EQ(renderCheckFailure(Tool, "src/a.cpp", notRun), renderFormatFailure(Tool, "src/a.cpp", notRun));
    EXPECT_EQ(renderCheckFailure(Tool, "src/a.cpp", scrap::Format::FileUnreadable{}),
              "error: cannot read 'src/a.cpp' to compare it with what clang-format wrote\n");
}
