#include "command/BuildProgress.h"

#include "build/BuildStep.h"
#include "compile/BuildProfile.h"

#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <string>

using scrap::Build::BuildStep;
using scrap::Build::StepKind;
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
