#include <gtest/gtest.h>

#include "build/BuildStep.h"
#include "build/SerialBuild.h"
#include "command/ProjectDiagnostic.h"
#include "compile/CompilationDatabase.h"
#include "project/LanguageStandard.h"
#include "project/ManifestError.h"
#include "project/ProjectLoader.h"
#include "project/SourceCollector.h"

#include <cerrno>
#include <optional>
#include <string>
#include <system_error>

using scrap::Build::BuildStep;
using scrap::Build::FailedStep;
using scrap::Build::StepFailure;
using scrap::Build::StepFailureKind;
using scrap::Build::StepKind;
using scrap::Command::renderCompilationDatabaseFailure;
using scrap::Command::renderLibraryNotBuilt;
using scrap::Command::renderNoCompilerFound;
using scrap::Command::renderNoTargetToBuild;
using scrap::Command::renderProjectError;
using scrap::Command::renderSourceScanFailure;
using scrap::Command::renderStepFailure;
using scrap::Command::renderUnsupportedStandard;
using scrap::Command::renderUnusableCompilerRequest;
using scrap::Compile::DatabaseWriteFailure;
using scrap::Compile::DatabaseWriteStep;
using scrap::Project::LanguageStandard;
using scrap::Project::ManifestError;
using scrap::Project::ManifestErrorKind;
using scrap::Project::NotADirectory;
using scrap::Project::PathInaccessible;
using scrap::Project::ProjectError;
using scrap::Project::ProjectNotFound;
using scrap::Project::SourcePosition;
using scrap::Project::SourceScanFailure;

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
 * A system with no compiler names both ways to give it one: installing a
 * compiler, or pointing the environment at the one to use.
 */
TEST(ProjectDiagnosticTest, RendersASystemWithNoCompiler)
{
    EXPECT_EQ(renderNoCompilerFound(),
              "error: no C++ compiler found\n"
              "hint: install a C++ compiler, or set CXX to the one to use\n");
}

/**
 * A compiler the environment asked for and cannot be run names the value it
 * gave, and both ways out: pointing CXX at a program, or leaving it unset.
 */
TEST(ProjectDiagnosticTest, RendersACompilerRequestThatCannotBeRun)
{
    EXPECT_EQ(renderUnusableCompilerRequest("ccache g++"),
              "error: CXX names 'ccache g++', which cannot be run\n"
              "hint: set CXX to the path of a compiler, or unset it to search for one\n");
}

/**
 * The value reaches the terminal as text, since the environment can hold any
 * byte and an escape sequence would otherwise be acted on.
 */
TEST(ProjectDiagnosticTest, EscapesAControlCharacterInACompilerRequest)
{
    EXPECT_EQ(renderUnusableCompilerRequest("g++\x1b[31m"),
              "error: CXX names 'g++\\x1B[31m', which cannot be run\n"
              "hint: set CXX to the path of a compiler, or unset it to search for one\n");
}

/**
 * A project with nothing to build names its root, and both ways to give it a
 * target: declaring one, or placing the file the default layout expects.
 */
TEST(ProjectDiagnosticTest, RendersAProjectWithNoTargetToBuild)
{
    EXPECT_EQ(renderNoTargetToBuild("/home/me/work/hello"),
              "error: no target to build in '/home/me/work/hello'\n"
              "hint: add a [[bin]] or [[lib]] section to scrap.toml, or create src/main.cpp\n");
}

/**
 * A source directory that cannot be read is named with the system's reason,
 * and points at the permissions.
 */
TEST(ProjectDiagnosticTest, RendersASourceDirectoryThatCannotBeRead)
{
    const SourceScanFailure failure{.directory = "/home/me/hello/src/private", .reason = "Permission denied"};

    EXPECT_EQ(renderSourceScanFailure(failure),
              "error: cannot read '/home/me/hello/src/private': Permission denied\n"
              "hint: check the permissions of the path\n");
}

/**
 * The directory is a path the user named, so it reaches the terminal as text.
 */
TEST(ProjectDiagnosticTest, EscapesAControlCharacterInASourceDirectory)
{
    const SourceScanFailure failure{.directory = "/home/me/hello/src/a\x1b[31m", .reason = "Permission denied"};

    EXPECT_EQ(renderSourceScanFailure(failure),
              "error: cannot read '/home/me/hello/src/a\\x1B[31m': Permission denied\n"
              "hint: check the permissions of the path\n");
}

/**
 * A build directory that cannot be created is named with the system's reason.
 * The reason is the platform's wording, so it is read back from the code.
 */
TEST(ProjectDiagnosticTest, RendersABuildDirectoryThatCannotBeCreated)
{
    const auto code = std::make_error_code(std::errc::permission_denied);
    const DatabaseWriteFailure failure{
        .step = DatabaseWriteStep::CreateDirectory, .path = "/home/me/hello/build/debug", .code = code};

    EXPECT_EQ(renderCompilationDatabaseFailure(failure),
              "error: cannot create '/home/me/hello/build/debug': " + code.message() +
                  "\n"
                  "hint: check the permissions of the path\n");
}

/**
 * A database that cannot be written is named with the system's reason, and a
 * full disk points at freeing space.
 */
TEST(ProjectDiagnosticTest, RendersADatabaseThatCannotBeWritten)
{
    const auto code = std::make_error_code(std::errc::no_space_on_device);
    const DatabaseWriteFailure failure{
        .step = DatabaseWriteStep::WriteFile, .path = "/home/me/hello/build/debug/compile_commands.json", .code = code};

    EXPECT_EQ(renderCompilationDatabaseFailure(failure),
              "error: cannot write '/home/me/hello/build/debug/compile_commands.json': " + code.message() +
                  "\n"
                  "hint: free some disk space and run the command again\n");
}

/**
 * The hint follows what the system reported: something already in the way,
 * a used-up quota, and anything else, a read-only file system among them.
 */
TEST(ProjectDiagnosticTest, ChoosesTheHintForABuildOutputByTheReason)
{
    const auto hintFor = [](const std::error_code& code) {
        const std::string text = renderCompilationDatabaseFailure(
            DatabaseWriteFailure{.step = DatabaseWriteStep::CreateDirectory, .path = "/p/build/debug", .code = code});
        return text.substr(text.find("\nhint: ") + 1);
    };

    EXPECT_EQ(hintFor(std::make_error_code(std::errc::not_a_directory)), "hint: check what is already at that path\n");
    EXPECT_EQ(hintFor(std::make_error_code(std::errc::file_exists)), "hint: check what is already at that path\n");
    EXPECT_EQ(hintFor(std::make_error_code(std::errc::is_a_directory)), "hint: check what is already at that path\n");
    EXPECT_EQ(hintFor(std::make_error_code(std::errc::operation_not_permitted)),
              "hint: check the permissions of the path\n");
    EXPECT_EQ(hintFor(std::make_error_code(std::errc::read_only_file_system)),
              "hint: check that the project directory can be written\n");
#ifdef EDQUOT
    EXPECT_EQ(hintFor(std::error_code(EDQUOT, std::generic_category())),
              "hint: free some disk space and run the command again\n");
#endif
}

/**
 * The path is one the user's project lives at, so it reaches the terminal as
 * text.
 */
TEST(ProjectDiagnosticTest, EscapesAControlCharacterInABuildOutputPath)
{
    const auto code = std::make_error_code(std::errc::permission_denied);
    const DatabaseWriteFailure failure{
        .step = DatabaseWriteStep::CreateDirectory, .path = "/home/me/a\x1b[31m/build/debug", .code = code};

    EXPECT_EQ(renderCompilationDatabaseFailure(failure),
              "error: cannot create '/home/me/a\\x1B[31m/build/debug': " + code.message() +
                  "\n"
                  "hint: check the permissions of the path\n");
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

namespace {

/**
 * A step that compiles @p source for the target "hello".
 */
auto compileStep(const char* source) -> BuildStep
{
    return BuildStep{.kind = StepKind::Compile,
                     .target = "hello",
                     .subject = source,
                     .directory = "/home/me/hello",
                     .output = "build/debug/obj/hello/src/main.cpp.o",
                     .arguments = {"/usr/bin/c++"}};
}

/**
 * A step that links the executable of the target "hello".
 */
auto linkStep() -> BuildStep
{
    return BuildStep{.kind = StepKind::Link,
                     .target = "hello",
                     .subject = "build/debug/bin/hello",
                     .directory = "/home/me/hello",
                     .output = "build/debug/bin/hello",
                     .arguments = {"/usr/bin/c++"}};
}

}  // namespace

/**
 * A compilation that failed names the source and the target it was building,
 * by the absolute path; the compiler has already said what is wrong.
 */
TEST(ProjectDiagnosticTest, RendersASourceThatFailedToCompile)
{
    const FailedStep failed{
        .step = compileStep("src/main.cpp"),
        .failure = StepFailure{.kind = StepFailureKind::Exited, .path = {}, .code = {}, .status = 1}};

    EXPECT_EQ(renderStepFailure(failed),
              "error: failed to compile '/home/me/hello/src/main.cpp' for 'hello'\n"
              "hint: fix the errors reported above and run the command again\n");
}

/**
 * A source written as ./<path>, so the compiler reads it as a file, is
 * reported by the path it names.
 */
TEST(ProjectDiagnosticTest, RendersASourceThatLooksLikeAnOption)
{
    const FailedStep failed{
        .step = compileStep("./-x.cpp"),
        .failure = StepFailure{.kind = StepFailureKind::Exited, .path = {}, .code = {}, .status = 1}};

    EXPECT_NE(renderStepFailure(failed).find("'/home/me/hello/-x.cpp'"), std::string::npos);
}

/**
 * A link that failed names the executable it was writing.
 */
TEST(ProjectDiagnosticTest, RendersAnExecutableThatFailedToLink)
{
    const FailedStep failed{
        .step = linkStep(),
        .failure = StepFailure{.kind = StepFailureKind::Exited, .path = {}, .code = {}, .status = 1}};

    EXPECT_EQ(renderStepFailure(failed),
              "error: failed to link '/home/me/hello/build/debug/bin/hello'\n"
              "hint: fix the errors reported above and run the command again\n");
}

/**
 * A compiler a signal stopped wrote no diagnostic of its own, so the signal
 * is named along with a step to take.
 */
TEST(ProjectDiagnosticTest, RendersACompilerASignalStopped)
{
    const FailedStep failed{
        .step = compileStep("src/main.cpp"),
        .failure = StepFailure{.kind = StepFailureKind::Signalled, .path = {}, .code = {}, .status = 9}};

    EXPECT_EQ(renderStepFailure(failed),
              "error: failed to compile '/home/me/hello/src/main.cpp' for 'hello': "
              "the compiler was stopped by signal 9\n"
              "hint: check that the compiler has enough memory and run the command again\n");
}

/**
 * A compiler that could not be started is named with the system's reason.
 */
TEST(ProjectDiagnosticTest, RendersACompilerThatCouldNotStart)
{
    const FailedStep failed{.step = compileStep("src/main.cpp"),
                            .failure = StepFailure{.kind = StepFailureKind::CannotStart,
                                                   .path = "/usr/bin/c++",
                                                   .code = std::make_error_code(std::errc::permission_denied),
                                                   .status = 0}};

    EXPECT_EQ(renderStepFailure(failed),
              "error: cannot run '/usr/bin/c++': " + std::make_error_code(std::errc::permission_denied).message() +
                  "\nhint: check that the compiler can be run, or set CXX to another one\n");
}

/**
 * A directory the build could not create is reported like any other path it
 * writes, with the hint the reason calls for.
 */
TEST(ProjectDiagnosticTest, RendersADirectoryAStepCouldNotCreate)
{
    const FailedStep failed{.step = compileStep("src/main.cpp"),
                            .failure = StepFailure{.kind = StepFailureKind::CannotCreateDirectory,
                                                   .path = "/home/me/hello/build/debug/obj",
                                                   .code = std::make_error_code(std::errc::not_a_directory),
                                                   .status = 0}};

    const std::string text = renderStepFailure(failed);

    EXPECT_NE(text.find("error: cannot create '/home/me/hello/build/debug/obj': "), std::string::npos) << text;
    EXPECT_NE(text.find("\nhint: check what is already at that path\n"), std::string::npos) << text;
}

/**
 * A standard the compiler has no version of is reported before anything is
 * compiled, with the two ways out of it.
 */
TEST(ProjectDiagnosticTest, RendersAStandardTheCompilerCannotBuild)
{
    EXPECT_EQ(renderUnsupportedStandard("/usr/bin/g++", LanguageStandard::Cxx26),
              "error: '/usr/bin/g++' does not support C++26\n"
              "hint: use a newer compiler, or set std in scrap.toml to an older standard\n");
}

/**
 * A library is named with what removing its declaration would do.
 */
TEST(ProjectDiagnosticTest, RendersALibraryItDoesNotBuild)
{
    EXPECT_EQ(renderLibraryNotBuilt("core"),
              "error: building the library 'core' is not supported yet\n"
              "hint: remove the [[lib]] section from scrap.toml to build its sources into the executable\n");
}
