#include <gtest/gtest.h>

#include "command/ProjectDiagnostic.h"
#include "project/ManifestError.h"
#include "project/ProjectLoader.h"

#include <optional>

using scrap::Command::renderProjectError;
using scrap::Project::ManifestError;
using scrap::Project::ManifestErrorKind;
using scrap::Project::NotADirectory;
using scrap::Project::PathInaccessible;
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
 * A path that cannot be examined carries the system's reason.
 */
TEST(ProjectDiagnosticTest, RendersAPathThatCannotBeAccessed)
{
    const ProjectError error = PathInaccessible{.path = "/root/proj", .reason = "Permission denied"};

    EXPECT_EQ(renderProjectError(error),
              "error: cannot access '/root/proj': Permission denied\n"
              "hint: check the permissions of the path\n");
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
                                             .message = "required key is missing",
                                             .kind = ManifestErrorKind::Invalid};

    EXPECT_EQ(renderProjectError(error),
              "/home/me/app/scrap.toml:1:1: error: package.name: required key is missing\n"
              "hint: correct scrap.toml and run the command again\n");
}

/**
 * A manifest that could not be read points at the file, not at its contents.
 */
TEST(ProjectDiagnosticTest, RendersAnUnreadableManifest)
{
    const ProjectError error = ManifestError{.file = "/home/me/app/scrap.toml",
                                             .position = std::nullopt,
                                             .key = {},
                                             .message = "cannot open the manifest",
                                             .kind = ManifestErrorKind::Unreadable};

    EXPECT_EQ(renderProjectError(error),
              "/home/me/app/scrap.toml: error: cannot open the manifest\n"
              "hint: check that scrap.toml is a readable file\n");
}

/**
 * A manifest without a position is still judged by its kind: a missing
 * [package] table is a fault in the contents.
 */
TEST(ProjectDiagnosticTest, RendersAMissingTableAsAContentsError)
{
    const ProjectError error = ManifestError{.file = "/home/me/app/scrap.toml",
                                             .position = std::nullopt,
                                             .key = "package",
                                             .message = "required table is missing",
                                             .kind = ManifestErrorKind::Invalid};

    EXPECT_EQ(renderProjectError(error),
              "/home/me/app/scrap.toml: error: package: required table is missing\n"
              "hint: correct scrap.toml and run the command again\n");
}

/**
 * An explicitly empty path argument is named as such, with the path hint.
 */
TEST(ProjectDiagnosticTest, RendersAnEmptyPathArgument)
{
    EXPECT_EQ(scrap::Command::renderEmptyPathArgument(),
              "error: the path argument is empty\n"
              "hint: pass a directory inside a project, or omit the path to use the current directory\n");
}

/**
 * An empty project name is named as such, with the naming rule.
 */
TEST(ProjectDiagnosticTest, RendersAnEmptyProjectName)
{
    const scrap::Project::CreateProjectError error = scrap::Project::InvalidProjectName{.name = ""};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: the project name is empty\n"
              "hint: use up to 64 letters, digits, '-' and '_', starting with a letter\n");
}

/**
 * An invalid project name is quoted, with the naming rule.
 */
TEST(ProjectDiagnosticTest, RendersAnInvalidProjectName)
{
    const scrap::Project::CreateProjectError error = scrap::Project::InvalidProjectName{.name = "a/b"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: 'a/b' is not a valid project name\n"
              "hint: use up to 64 letters, digits, '-' and '_', starting with a letter\n");
}

/**
 * Bytes a terminal would act on are shown as \xNN instead of being written raw.
 */
TEST(ProjectDiagnosticTest, EscapesControlCharactersInAProjectName)
{
    const scrap::Project::CreateProjectError error = scrap::Project::InvalidProjectName{.name = "a\nb\x1b[2J"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: 'a\\x0Ab\\x1B[2J' is not a valid project name\n"
              "hint: use up to 64 letters, digits, '-' and '_', starting with a letter\n");
}

/**
 * Bytes outside ASCII are shown as \xNN as well.
 */
TEST(ProjectDiagnosticTest, EscapesNonAsciiBytesInAProjectName)
{
    const scrap::Project::CreateProjectError error = scrap::Project::InvalidProjectName{.name = "caf\xc3\xa9"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: 'caf\\xC3\\xA9' is not a valid project name\n"
              "hint: use up to 64 letters, digits, '-' and '_', starting with a letter\n");
}

/**
 * An existing path is named, with a next step that keeps it intact.
 */
TEST(ProjectDiagnosticTest, RendersAnExistingPath)
{
    const scrap::Project::CreateProjectError error = scrap::Project::PathExists{.path = "/home/me/work/hello"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: '/home/me/work/hello' already exists\n"
              "hint: choose another name, or run the command in another directory\n");
}

/**
 * A path that cannot be created carries the system's reason.
 */
TEST(ProjectDiagnosticTest, RendersAPathThatCannotBeCreated)
{
    const scrap::Project::CreateProjectError error =
        scrap::Project::CannotCreate{.path = "/home/me/work/hello", .reason = "Permission denied"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: cannot create '/home/me/work/hello': Permission denied\n"
              "hint: check the permissions of the path\n");
}
