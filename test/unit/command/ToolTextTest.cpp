#include "command/ToolText.h"

#include "tool/ExternalTool.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <system_error>

using namespace scrap::Command;
using scrap::Tool::ExternalTool;
using scrap::Tool::ToolFailed;
using scrap::Tool::ToolNotRun;

TEST(ToolTextTest, NamesTheToolInUseWithItsVersion)
{
    EXPECT_EQ(renderToolInUse(ExternalTool{ .name = "clang-format", .program = "/usr/bin/clang-format", .version = "18.1.3" }),
              "Using clang-format '/usr/bin/clang-format' (version 18.1.3)\n");
    EXPECT_EQ(renderToolInUse(ExternalTool{ .name = "clang-tidy", .program = "/opt/ct", .version = "unknown" }), "Using clang-tidy '/opt/ct' (version unknown)\n");
}

TEST(ToolTextTest, SaysNoToolWasFound)
{
    EXPECT_EQ(renderToolNotFound("clang-format"),
              "error: clang-format not found in PATH\n"
              "hint: install clang-format and make sure it is on PATH\n");
    EXPECT_EQ(renderToolNotFound("clang-tidy"),
              "error: clang-tidy not found in PATH\n"
              "hint: install clang-tidy and make sure it is on PATH\n");
}

TEST(ToolTextTest, RendersAVersionThatCouldNotBeLearned)
{
    const ToolFailed failed{ .exitCode = 2, .signal = std::nullopt, .output = "bad option\n" };
    const ToolNotRun notRun{ .code = std::make_error_code(std::errc::permission_denied) };

    EXPECT_EQ(renderVersionFailure("clang-format", "/usr/bin/clang-format", failed), "error: cannot learn the version of '/usr/bin/clang-format'\nbad option\n");
    EXPECT_EQ(renderVersionFailure("clang-format", "/usr/bin/clang-format", notRun), "error: cannot run '/usr/bin/clang-format': " + notRun.code.message() + "\n");
}

/**
 * A program that failed is named under the heading with the signal that
 * stopped it, or with its exit status when it wrote nothing.
 */
TEST(ToolTextTest, RendersAToolThatFailedUnderTheHeading)
{
    const ToolFailed stopped{ .exitCode = std::nullopt, .signal = 9, .output = "" };
    const ToolFailed silent{ .exitCode = 3, .signal = std::nullopt, .output = "" };
    const ToolFailed talking{ .exitCode = 1, .signal = std::nullopt, .output = "no such check" };
    const ExternalTool tidy{ .name = "clang-tidy", .program = "/usr/bin/clang-tidy", .version = "18.1.3" };

    EXPECT_EQ(renderToolFailure(tidy, "error: failed", stopped), "error: failed: clang-tidy was stopped by signal 9\n");
    EXPECT_EQ(renderToolFailure(tidy, "error: failed", silent), "error: failed: clang-tidy exited with status 3\n");
    EXPECT_EQ(renderToolFailure(tidy, "error: failed", talking), "error: failed\nno such check\n");
}
