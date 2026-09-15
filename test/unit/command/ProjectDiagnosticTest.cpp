#include <gtest/gtest.h>

#include "command/ProjectDiagnostic.h"
#include "project/ManifestError.h"
#include "project/ProjectLoader.h"

using scrap::Command::renderProjectError;
using scrap::Project::ManifestError;
using scrap::Project::NotADirectory;
using scrap::Project::ProjectError;
using scrap::Project::ProjectNotFound;
using scrap::Project::SourcePosition;

/**
 * A start path that is not a directory is named, with how to pass one.
 */
TEST(ProjectDiagnosticTest, RendersAPathThatIsNotADirectory)
{
    const ProjectError error = NotADirectory{.path = "/home/me/typo"};

    EXPECT_EQ(renderProjectError(error),
              "error: '/home/me/typo' is not a directory\n"
              "hint: pass a directory inside a project, or omit the path to use the current directory\n");
}

/**
 * A missing project names the directory searched from, with how to make one.
 */
TEST(ProjectDiagnosticTest, RendersAMissingProject)
{
    const ProjectError error = ProjectNotFound{.startDir = "/home/me/work"};

    EXPECT_EQ(renderProjectError(error),
              "error: could not find scrap.toml in '/home/me/work' or any parent directory\n"
              "hint: run 'scrap new <name>' to create a project\n");
}

/**
 * A manifest error keeps its compiler-style location line.
 */
TEST(ProjectDiagnosticTest, RendersAManifestErrorWithItsLocation)
{
    const ProjectError error = ManifestError{.file = "/home/me/app/scrap.toml",
                                             .position = SourcePosition{.line = 1, .column = 1},
                                             .key = "package.name",
                                             .message = "required key is missing"};

    EXPECT_EQ(renderProjectError(error),
              "/home/me/app/scrap.toml:1:1: error: package.name: required key is missing\n"
              "hint: correct scrap.toml and run the command again\n");
}
