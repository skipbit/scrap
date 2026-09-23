#include "command/BuildProgress.h"

#include "build/BuildStep.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using scrap::Build::BuildStep;
using scrap::Build::StepKind;
using scrap::Command::printableOutput;
using scrap::Command::renderBuildFinished;
using scrap::Command::StreamBuildReporter;

namespace {

BuildStep compileStep()
{
    return BuildStep{ .kind = StepKind::Compile,
                      .target = "hello",
                      .subject = "src/main.cpp",
                      .directory = "/home/me/hello",
                      .output = "build/debug/obj/hello/src/main.cpp.o",
                      .arguments = {} };
}

BuildStep linkStep()
{
    return BuildStep{ .kind = StepKind::Link,
                      .target = "hello",
                      .subject = "build/debug/bin/hello",
                      .directory = "/home/me/hello",
                      .output = "build/debug/bin/hello",
                      .arguments = {} };
}

}  // namespace

/**
 * A step says what it is doing, to what, in lines that line up.
 */
TEST(BuildProgressTest, WritesALineForEachStep)
{
    std::ostringstream out;
    StreamBuildReporter reporter{ out, false };

    reporter.started(compileStep());
    reporter.started(linkStep());

    EXPECT_EQ(out.str(),
              "   Compiling hello (src/main.cpp)\n"
              "     Linking hello (build/debug/bin/hello)\n");
}

/**
 * What the compiler wrote follows the step it belongs to, and ends in a
 * newline whether or not the compiler wrote one.
 */
TEST(BuildProgressTest, WritesWhatTheProgramWrote)
{
    std::ostringstream out;
    StreamBuildReporter reporter{ out, false };

    reporter.finished(compileStep(), "src/main.cpp:1:1: warning: unused");

    EXPECT_EQ(out.str(), "src/main.cpp:1:1: warning: unused\n");
}

/**
 * A step that wrote nothing adds nothing.
 */
TEST(BuildProgressTest, WritesNothingForAProgramThatWroteNothing)
{
    std::ostringstream out;
    StreamBuildReporter reporter{ out, false };

    reporter.finished(compileStep(), "");

    EXPECT_TRUE(out.str().empty());
}

/**
 * Colour is left in where the output is a terminal, and taken out where it
 * is not.
 */
TEST(BuildProgressTest, KeepsColorOnlyWhereItWasAskedFor)
{
    const std::string colored = "\x1b[01;31m\x1b[Kerror\x1b[m\x1b[K: boom\n";
    std::ostringstream kept;
    std::ostringstream removed;

    StreamBuildReporter{ kept, true }.finished(compileStep(), colored);
    StreamBuildReporter{ removed, false }.finished(compileStep(), colored);

    EXPECT_EQ(kept.str(), colored);
    EXPECT_EQ(removed.str(), "error: boom\n");
}

/**
 * A name or a path the terminal would read as instructions is escaped, as
 * every other message escapes them.
 */
TEST(BuildProgressTest, EscapesWhatItCannotPrint)
{
    std::ostringstream out;
    StreamBuildReporter reporter{ out, false };
    BuildStep step = compileStep();
    step.target = "he\x1b[31mllo";

    reporter.started(step);

    EXPECT_EQ(out.str(), "   Compiling he\\x1B[31mllo (src/main.cpp)\n");
}

/**
 * The sequences that colour are kept or removed as asked; text around them
 * is left as it is.
 */
TEST(BuildProgressTest, KeepsOrRemovesTheSequencesThatColor)
{
    EXPECT_EQ(printableOutput("plain text", false), "plain text");
    EXPECT_EQ(printableOutput("\x1b[01;31mred\x1b[0m", false), "red");
    EXPECT_EQ(printableOutput("\x1b[01;31mred\x1b[0m", true), "\x1b[01;31mred\x1b[0m");
    EXPECT_EQ(printableOutput("gcc\x1b[Kclears the line", false), "gccclears the line");
    EXPECT_EQ(printableOutput("gcc\x1b[Kclears the line", true), "gcc\x1b[Kclears the line");
}

/**
 * A diagnostic quotes the source it read, so what it carries is the
 * project's to decide: every other sequence reaches the terminal as text,
 * whether or not colour is kept.
 */
TEST(BuildProgressTest, WritesEveryOtherSequenceAsText)
{
    for (const bool keepColor : { false, true }) {
        // Rewriting the terminal's title, clearing its screen, and a byte
        // that starts neither.
        EXPECT_EQ(printableOutput("\x1b]0;pwned\x07"
                                  "after",
                                  keepColor),
                  "\\x1B]0;pwned\x07"
                  "after")
            << keepColor;
        EXPECT_EQ(printableOutput("\x1b[2Jgone", keepColor), "\\x1B[2Jgone") << keepColor;
        EXPECT_EQ(printableOutput("a lone \x1b stays", keepColor), "a lone \\x1B stays") << keepColor;
    }
}

/**
 * A build that reached the end says so, in the same lined-up form.
 */
TEST(BuildProgressTest, SaysWhenTheBuildIsDone)
{
    EXPECT_EQ(renderBuildFinished(), "    Finished debug build\n");
}
