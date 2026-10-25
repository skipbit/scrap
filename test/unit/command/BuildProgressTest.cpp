#include "command/BuildProgress.h"

#include "build/BuildStep.h"
#include "compile/BuildProfile.h"

#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <string>

using scrap::Build::BuildStep;
using scrap::Build::StepKind;
using scrap::Command::printableOutput;
using scrap::Command::renderBuildFinished;
using scrap::Command::renderRunning;
using scrap::Command::StreamBuildReporter;
using scrap::Compile::BuildProfile;

namespace {

BuildStep compileStep()
{
    return BuildStep{ .kind = StepKind::Compile,
                      .target = "hello",
                      .subject = "src/main.cpp",
                      .directory = "/home/me/hello",
                      .output = "build/debug/obj/hello/src/main.cpp.o",
                      .arguments = {},
                      .inputs = {},
                      .searchedDirectories = {},
                      .dependencyFile = std::nullopt };
}

BuildStep archiveStep()
{
    return BuildStep{ .kind = StepKind::Archive,
                      .target = "core",
                      .subject = "build/debug/lib/libcore.a",
                      .directory = "/home/me/hello",
                      .output = "build/debug/lib/libcore.a",
                      .arguments = {},
                      .inputs = {},
                      .searchedDirectories = {},
                      .dependencyFile = std::nullopt };
}

BuildStep linkStep()
{
    return BuildStep{ .kind = StepKind::Link,
                      .target = "hello",
                      .subject = "build/debug/bin/hello",
                      .directory = "/home/me/hello",
                      .output = "build/debug/bin/hello",
                      .arguments = {},
                      .inputs = {},
                      .searchedDirectories = {},
                      .dependencyFile = std::nullopt };
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
    reporter.started(archiveStep());
    reporter.started(linkStep());

    EXPECT_EQ(out.str(),
              "   Compiling hello (src/main.cpp)\n"
              "   Archiving core (build/debug/lib/libcore.a)\n"
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
                  "\\x1B]0;pwned\\x07"
                  "after")
            << keepColor;
        EXPECT_EQ(printableOutput("\x1b[2Jgone", keepColor), "\\x1B[2Jgone") << keepColor;
        EXPECT_EQ(printableOutput("a lone \x1b stays", keepColor), "a lone \\x1B stays") << keepColor;
    }
}

/**
 * Only the forms a compiler colours with pass as colour: a sequence that
 * erases more than the rest of the line, or takes a parameter no colour
 * takes, is written as text.
 */
TEST(BuildProgressTest, PassesOnlyTheFormsACompilerColoursWith)
{
    for (const bool keepColor : { false, true }) {
        EXPECT_EQ(printableOutput("a\x1b[2Kb", keepColor), "a\\x1B[2Kb") << keepColor;
        EXPECT_EQ(printableOutput("a\x1b[1Kb", keepColor), "a\\x1B[1Kb") << keepColor;
        EXPECT_EQ(printableOutput("a\x1b[?1mb", keepColor), "a\\x1B[?1mb") << keepColor;
    }
    EXPECT_EQ(printableOutput("\x1b[01;35m\x1b[Ka\x1b[0Kb\x1b[m", true), "\x1b[01;35m\x1b[Ka\x1b[0Kb\x1b[m");
    EXPECT_EQ(printableOutput("\x1b[0;1;31ma\x1b[0m", true), "\x1b[0;1;31ma\x1b[0m");
}

/**
 * A byte that is not an escape can still be one a terminal acts on, so
 * everything printableText() escapes is written as text here too, colour or
 * not. The bytes a diagnostic is laid out with and quotes source in stay.
 */
TEST(BuildProgressTest, WritesEveryOtherControlAsText)
{
    for (const bool keepColor : { false, true }) {
        // CSI and OSC in an eight-bit locale, the bell, a backspace, a
        // carriage return and an overlong encoding.
        EXPECT_EQ(printableOutput("\x9b"
                                  "2J \x9d"
                                  "0;t \x07 \x08 \r \xc0\x9b",
                                  keepColor),
                  "\\x9B2J \\x9D0;t \\x07 \\x08 \\x0D \\xC0\\x9B")
            << keepColor;
        EXPECT_EQ(printableOutput(std::string{ 'a', '\xe2', '\x80', '\xae', 'b' }, keepColor), "a\\xE2\\x80\\xAEb") << keepColor;
        // Lines, a tab, a quoted backslash and the quotes gcc writes in UTF-8.
        EXPECT_EQ(printableOutput("1 |\tprintf(\"a\\n\");\n\xe2\x80\x98x\xe2\x80\x99\n", keepColor), "1 |\tprintf(\"a\\n\");\n\xe2\x80\x98x\xe2\x80\x99\n") << keepColor;
    }
    EXPECT_EQ(printableOutput("\x1b[01;31m\x9b\x1b[0m", true), "\x1b[01;31m\\x9B\x1b[0m");
}

/**
 * A build that reached the end says so, and for which profile, in the same
 * lined-up form.
 */
TEST(BuildProgressTest, SaysWhenTheBuildIsDone)
{
    EXPECT_EQ(renderBuildFinished(BuildProfile::Debug), "    Finished debug build\n");
    EXPECT_EQ(renderBuildFinished(BuildProfile::Release), "    Finished release build\n");
}

/**
 * The executable about to be started is named in the same lined-up form.
 */
TEST(BuildProgressTest, SaysWhichExecutableIsStarted)
{
    EXPECT_EQ(renderRunning("hello", "build/debug/bin/hello"), "     Running hello (build/debug/bin/hello)\n");
}
