#include "command/ProjectPathsText.h"

#include "command/ProjectArgument.h"
#include "command/ProjectDiagnostic.h"
#include "command/ProjectPaths.h"
#include "project/ProjectLoader.h"

#include <gtest/gtest.h>

using namespace scrap::Command;

TEST(ProjectPathsTextTest, RendersAPathThatNamesNothing)
{
    EXPECT_EQ(renderProjectPathsError(PathMissing{ .path = "src/nope.cpp" }, "format"),
              "error: 'src/nope.cpp' does not exist\n"
              "hint: pass a file or directory inside the project, or omit the path to format the whole project\n");
}

TEST(ProjectPathsTextTest, RendersAnEmptyPath)
{
    EXPECT_EQ(renderProjectPathsError(EmptyPathArgument{}, "check"),
              "error: the path argument is empty\n"
              "hint: pass a file or directory inside the project, or omit the path to check the whole project\n");
}

TEST(ProjectPathsTextTest, RendersAPathOutsideTheProject)
{
    EXPECT_EQ(renderProjectPathsError(PathOutsideProject{ .path = "/tmp/x.cpp", .projectRoot = "/home/me/proj" }, "format"),
              "error: '/tmp/x.cpp' is not in the project at '/home/me/proj'\n"
              "hint: run scrap format once for each project\n");
}

/**
 * A project that could not be loaded reads as it does for any project
 * command.
 */
TEST(ProjectPathsTextTest, RendersAProjectErrorAsTheProjectCommandsDo)
{
    const scrap::Project::ProjectError error = scrap::Project::ProjectNotFound{ .startDir = "/home/me/loose" };

    EXPECT_EQ(renderProjectPathsError(error, "format"), renderProjectError(error));
}

TEST(ProjectPathsTextTest, RendersAFileTheCommandDoesNotCover)
{
    EXPECT_EQ(renderNotCovered("tools/gen.cpp", "format"), "warning: leaving out 'tools/gen.cpp', which scrap format does not cover\n");
}
