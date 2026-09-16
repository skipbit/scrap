#include <gtest/gtest.h>

#include "command/ProjectDiagnostic.h"
#include "project/ManifestError.h"
#include "project/ProjectLoader.h"

#include <cerrno>
#include <optional>
#include <system_error>

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
              "hint: run 'scrap new <project-name>' to create a project\n");
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
 * A backslash in a name is escaped too, so the escape marks an escaped byte
 * alone.
 */
TEST(ProjectDiagnosticTest, EscapesABackslashInAProjectName)
{
    const scrap::Project::CreateProjectError error = scrap::Project::InvalidProjectName{.name = "a\\x0Ab"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: 'a\\x5Cx0Ab' is not a valid project name\n"
              "hint: use up to 64 letters, digits, '-' and '_', starting with a letter\n");
}

/**
 * A control character in a path is escaped, wherever the path came from.
 */
TEST(ProjectDiagnosticTest, EscapesControlCharactersInAPath)
{
    const scrap::Project::CreateProjectError error = scrap::Project::PathExists{.path = "/home/me/w\nork/hello"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: '/home/me/w\\x0Aork/hello' already exists\n"
              "hint: choose another name, or run the command in another directory\n");
}

/**
 * The two bytes of a C1 control character are escaped together.
 */
TEST(ProjectDiagnosticTest, EscapesAC1ControlCharacterInAPath)
{
    const scrap::Project::CreateProjectError error = scrap::Project::PathExists{.path = "/home/me/a\xc2\x9b"
                                                                                        "b/hello"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: '/home/me/a\\xC2\\x9Bb/hello' already exists\n"
              "hint: choose another name, or run the command in another directory\n");
}

/**
 * A byte in the C1 range on its own is escaped, since a terminal in an
 * eight-bit locale acts on it as a control character.
 */
TEST(ProjectDiagnosticTest, EscapesALoneByteInTheC1Range)
{
    const scrap::Project::CreateProjectError error = scrap::Project::PathExists{.path = "/home/me/a\x9b"
                                                                                        "b/hello"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: '/home/me/a\\x9Bb/hello' already exists\n"
              "hint: choose another name, or run the command in another directory\n");
}

/**
 * A byte that starts no well-formed sequence is escaped as well.
 */
TEST(ProjectDiagnosticTest, EscapesBytesThatFormNoUtf8Sequence)
{
    EXPECT_EQ(scrap::Command::printablePath("/home/me/\xe4\xbd/hello"), "/home/me/\\xE4\\xBD/hello");
    EXPECT_EQ(scrap::Command::printablePath("/home/me/\xff/hello"), "/home/me/\\xFF/hello");
}

/**
 * A name longer than a project name may be is cut where it stops mattering.
 */
TEST(ProjectDiagnosticTest, CutsALongNameInTheMessage)
{
    const std::string name(scrap::Project::MaxProjectNameLength + 20, 'a');
    const scrap::Project::CreateProjectError error = scrap::Project::InvalidProjectName{.name = name};

    const std::string message = scrap::Command::renderCreateProjectError(error);

    EXPECT_NE(message.find("error: '" + std::string(scrap::Project::MaxProjectNameLength, 'a') + "...' is not a valid"),
              std::string::npos)
        << message;
}

/**
 * Letters outside ASCII stay as they are, so a path keeps its own language.
 */
TEST(ProjectDiagnosticTest, KeepsLettersOutsideAsciiInAPath)
{
    const scrap::Project::CreateProjectError error =
        scrap::Project::PathExists{.path = "/home/me/\xe4\xbd\x9c\xe6\xa5\xad/hello"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: '/home/me/\xe4\xbd\x9c\xe6\xa5\xad/hello' already exists\n"
              "hint: choose another name, or run the command in another directory\n");
}

/**
 * A quota that is used up points at freeing space, as a full disk does.
 */
TEST(ProjectDiagnosticTest, RendersAnExceededQuotaWithTheDiskSpaceHint)
{
#ifdef EDQUOT
    const scrap::Project::CreateProjectError error =
        scrap::Project::CannotCreate{.path = "/home/me/work/hello/src/main.cpp",
                                     .reason = "Disk quota exceeded",
                                     .code = std::error_code(EDQUOT, std::generic_category()),
                                     .leftBehind = std::nullopt};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: cannot create '/home/me/work/hello/src/main.cpp': Disk quota exceeded\n"
              "hint: free some disk space and run the command again\n");
#else
    GTEST_SKIP() << "EDQUOT is not defined on this system";
#endif
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
 * A path refused for lack of permission carries the system's reason and points
 * at the permissions.
 */
TEST(ProjectDiagnosticTest, RendersAPathThatCannotBeCreated)
{
    const scrap::Project::CreateProjectError error =
        scrap::Project::CannotCreate{.path = "/home/me/work/hello",
                                     .reason = "Permission denied",
                                     .code = std::make_error_code(std::errc::permission_denied),
                                     .leftBehind = std::nullopt};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: cannot create '/home/me/work/hello': Permission denied\n"
              "hint: check the permissions of the path\n");
}

/**
 * A full disk points at freeing space.
 */
TEST(ProjectDiagnosticTest, RendersAFullDiskWithItsOwnHint)
{
    const scrap::Project::CreateProjectError error =
        scrap::Project::CannotCreate{.path = "/home/me/work/hello",
                                     .reason = "No space left on device",
                                     .code = std::make_error_code(std::errc::no_space_on_device),
                                     .leftBehind = std::nullopt};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: cannot create '/home/me/work/hello': No space left on device\n"
              "hint: free some disk space and run the command again\n");
}

/**
 * A read-only file system points at a writable directory.
 */
TEST(ProjectDiagnosticTest, RendersAReadOnlyFileSystemWithItsOwnHint)
{
    const scrap::Project::CreateProjectError error =
        scrap::Project::CannotCreate{.path = "/mnt/cdrom/hello",
                                     .reason = "Read-only file system",
                                     .code = std::make_error_code(std::errc::read_only_file_system),
                                     .leftBehind = std::nullopt};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: cannot create '/mnt/cdrom/hello': Read-only file system\n"
              "hint: run the command in a writable directory\n");
}

/**
 * A partly created project that could not be removed is named in the hint, so
 * the next attempt does not stop at a directory the user did not make.
 */
TEST(ProjectDiagnosticTest, RendersAPartlyCreatedProjectLeftBehind)
{
    const scrap::Project::CreateProjectError error =
        scrap::Project::CannotCreate{.path = "/home/me/work/hello/src/main.cpp",
                                     .reason = "No space left on device",
                                     .code = std::make_error_code(std::errc::no_space_on_device),
                                     .leftBehind = "/home/me/work/hello"};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: cannot create '/home/me/work/hello/src/main.cpp': No space left on device\n"
              "hint: remove the partly created '/home/me/work/hello' before trying again\n");
}

/**
 * A file already at the path, which creating it exclusively reports, points at
 * what is there rather than at the directory.
 */
TEST(ProjectDiagnosticTest, RendersAFileAlreadyAtThePath)
{
    const scrap::Project::CreateProjectError error =
        scrap::Project::CannotCreate{.path = "/home/me/work/hello/src/main.cpp",
                                     .reason = "File exists",
                                     .code = std::make_error_code(std::errc::file_exists),
                                     .leftBehind = std::nullopt};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: cannot create '/home/me/work/hello/src/main.cpp': File exists\n"
              "hint: check what is already at that path\n");
}

/**
 * A template file that leaves the project points at the template.
 */
TEST(ProjectDiagnosticTest, RendersATemplateFileOutsideTheProject)
{
    const scrap::Project::CreateProjectError error =
        scrap::Project::CannotCreate{.path = "/home/me/work/hello/../outside.txt",
                                     .reason = "the template file path leaves the project directory",
                                     .code = std::make_error_code(std::errc::invalid_argument),
                                     .leftBehind = std::nullopt};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(error),
              "error: cannot create '/home/me/work/hello/../outside.txt': the template file path leaves the project "
              "directory\n"
              "hint: choose a template whose files stay inside the project\n");
}

/**
 * Any other failure, including one with no code, gets the general hint.
 */
TEST(ProjectDiagnosticTest, RendersAnyOtherFailureWithTheGeneralHint)
{
    const scrap::Project::CreateProjectError missing =
        scrap::Project::CannotCreate{.path = "/home/me/gone/hello",
                                     .reason = "No such file or directory",
                                     .code = std::make_error_code(std::errc::no_such_file_or_directory),
                                     .leftBehind = std::nullopt};
    const scrap::Project::CreateProjectError uncoded =
        scrap::Project::CannotCreate{.path = "/home/me/work/hello/scrap.toml",
                                     .reason = "the file could not be written",
                                     .code = {},
                                     .leftBehind = std::nullopt};

    EXPECT_EQ(scrap::Command::renderCreateProjectError(missing),
              "error: cannot create '/home/me/gone/hello': No such file or directory\n"
              "hint: check that the directory exists and can be written\n");
    EXPECT_EQ(scrap::Command::renderCreateProjectError(uncoded),
              "error: cannot create '/home/me/work/hello/scrap.toml': the file could not be written\n"
              "hint: check that the directory exists and can be written\n");
}
