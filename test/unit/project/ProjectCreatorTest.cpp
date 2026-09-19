#include <gtest/gtest.h>

#include "project/DefaultTemplate.h"
#include "project/Manifest.h"
#include "project/ProjectCreator.h"
#include "project/ProjectFileSystem.h"
#include "project/ProjectLoader.h"
#include "project/ProjectLocator.h"
#include "project/TargetResolver.h"
#include "project/TemplateFile.h"
#include "project/driver/DiskProjectFileSystem.h"

#include "support/TempDirectory.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

using namespace scrap::Project;
using scrap::TestSupport::TempDirectory;

namespace {

/// The real file system, which these tests create projects on.
auto diskFileSystem() -> DiskProjectFileSystem&
{
    static DiskProjectFileSystem files;
    return files;
}

/**
 * A file system that answers with the failures a test asks for, so the paths
 * a real file system reaches only under a full disk or a lost permission can
 * be exercised.
 */
class FailingFileSystem : public ProjectFileSystem {
public:
    std::error_code absoluteError;
    std::error_code createDirectoryError;
    std::error_code createDirectoriesError;
    std::error_code writeError;
    std::error_code removeError;
    int removeCalls = 0;

    [[nodiscard]] auto
    absolute(const std::filesystem::path& path) const -> std::expected<std::filesystem::path, std::error_code> override
    {
        if (absoluteError) {
            return std::unexpected(absoluteError);
        }
        return std::filesystem::path{"/absolute"} / path.filename();
    }

    [[nodiscard]] auto createDirectory(const std::filesystem::path&) -> std::error_code override
    {
        return createDirectoryError;
    }

    [[nodiscard]] auto createDirectories(const std::filesystem::path&) -> std::error_code override
    {
        return createDirectoriesError;
    }

    [[nodiscard]] auto writeNewFile(const std::filesystem::path&, std::string_view) -> std::error_code override
    {
        return writeError;
    }

    [[nodiscard]] auto removeAll(const std::filesystem::path&) -> std::error_code override
    {
        ++removeCalls;
        return removeError;
    }
};

}  // namespace

/**
 * Names of an ASCII letter followed by letters, digits, '-' and '_' are valid.
 */
TEST(ProjectCreatorTest, AcceptsLettersDigitsHyphensAndUnderscores)
{
    for (const char* name : {"a", "hello", "my-app", "my_app", "App2", "x-1_y"}) {
        EXPECT_TRUE(isValidProjectName(name)) << name;
    }
}

/**
 * Every other name is rejected, including what scrap.toml alone would accept.
 */
TEST(ProjectCreatorTest, RejectsNamesOutsideTheRule)
{
    for (const char* name : {"",
                             "../x",
                             "a/b",
                             "a\\b",
                             ".",
                             "..",
                             ".hidden",
                             "1app",
                             "-app",
                             "_app",
                             "my app",
                             "a.b",
                             "a\"b",
                             "caf\u00e9"}) {
        EXPECT_FALSE(isValidProjectName(name)) << name;
    }
}

/**
 * A name may be exactly as long as the limit, and no longer.
 */
TEST(ProjectCreatorTest, LimitsTheNameTo64Characters)
{
    EXPECT_EQ(MaxProjectNameLength, 64U);
    const std::string longest = "a" + std::string(MaxProjectNameLength - 1, 'b');

    EXPECT_TRUE(isValidProjectName(longest));
    EXPECT_FALSE(isValidProjectName(longest + "c"));
}

/**
 * The project directory is created with every template file in it.
 */
TEST(ProjectCreatorTest, CreatesTheProjectWithItsFiles)
{
    const TempDirectory temp;

    const auto root = createProject(diskFileSystem(), temp.path(), "hello", defaultTemplateFiles);

    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(*root, temp.path() / "hello");
    EXPECT_EQ(temp.readFile(*root / "scrap.toml"), "[package]\nname = \"hello\"\nversion = \"0.1.0\"\nstd = \"23\"\n");
    EXPECT_TRUE(std::filesystem::is_regular_file(*root / "src/main.cpp"));
}

/**
 * A project from the built-in template loads, and its default layout yields
 * one executable named after the project.
 */
TEST(ProjectCreatorTest, DefaultTemplateLoadsAsAProject)
{
    const TempDirectory temp;
    const auto root = createProject(diskFileSystem(), temp.path(), "hello", defaultTemplateFiles);
    ASSERT_TRUE(root.has_value());

    const auto project = loadProject(*root);

    ASSERT_TRUE(project.has_value());
    EXPECT_EQ(project->manifest.package.name, "hello");
    EXPECT_EQ(project->manifest.package.version, "0.1.0");
    EXPECT_FALSE(project->manifest.declaresTargets);
    const auto targets = resolveTargets(project->root, project->manifest);
    ASSERT_EQ(targets.size(), 1U);
    EXPECT_EQ(targets[0].kind, TargetKind::Executable);
    EXPECT_EQ(targets[0].name, "hello");
    EXPECT_EQ(targets[0].entryPoint, "src/main.cpp");
}

/**
 * An existing directory is reported and keeps its contents.
 */
TEST(ProjectCreatorTest, LeavesAnExistingDirectoryUntouched)
{
    const TempDirectory temp;
    temp.writeFile("hello/marker.txt", "keep me\n");

    const auto root = createProject(diskFileSystem(), temp.path(), "hello", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<PathExists>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, temp.path() / "hello");
    EXPECT_EQ(temp.readFile("hello/marker.txt"), "keep me\n");
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "hello" / ManifestFileName));
}

/**
 * A file where the directory would go is reported the same way.
 */
TEST(ProjectCreatorTest, ReportsAnExistingFileAsExisting)
{
    const TempDirectory temp;
    temp.writeFile("hello", "a file\n");

    const auto root = createProject(diskFileSystem(), temp.path(), "hello", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    EXPECT_NE(std::get_if<PathExists>(&root.error()), nullptr);
    EXPECT_EQ(temp.readFile("hello"), "a file\n");
}

/**
 * An invalid name leaves the file system as it was, including the path the
 * name points at.
 */
TEST(ProjectCreatorTest, RejectsAnInvalidNameWithoutCreatingAnything)
{
    const TempDirectory temp;
    temp.makeDirectory("work");

    const auto root = createProject(diskFileSystem(), temp.path() / "work", "../x", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<InvalidProjectName>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->name, "../x");
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "x"));
    EXPECT_TRUE(std::filesystem::is_empty(temp.path() / "work"));
}

/**
 * The template is asked for its files only once the name is valid, so every
 * name a template sees is one it can write as it is.
 */
TEST(ProjectCreatorTest, BuildsTemplateFilesOnlyForAValidName)
{
    const TempDirectory temp;
    bool called = false;
    const auto recordCall = [&called](std::string_view) {
        called = true;
        return std::vector<TemplateFile>{};
    };

    const auto rejected = createProject(diskFileSystem(), temp.path(), "a\"b", recordCall);

    EXPECT_FALSE(rejected.has_value());
    EXPECT_FALSE(called);

    const auto created = createProject(diskFileSystem(), temp.path(), "hello", recordCall);

    EXPECT_TRUE(created.has_value());
    EXPECT_TRUE(called);
}

/**
 * A file that cannot be written removes the project directory created for it.
 */
TEST(ProjectCreatorTest, RemovesThePartialProjectWhenAFileCannotBeWritten)
{
    const TempDirectory temp;

    const auto root = createProject(diskFileSystem(), temp.path(), "hello", [](std::string_view) {
        return std::vector<TemplateFile>{TemplateFile{.path = "a", .content = "a file\n"},
                                         TemplateFile{.path = "a/b", .content = "under a file\n"}};
    });

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<CannotCreate>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_FALSE(error->reason.empty());
    EXPECT_TRUE(static_cast<bool>(error->code));
    EXPECT_FALSE(error->leftBehind.has_value());
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "hello"));
}

/**
 * A name that is already taken leaves the template unasked, since its files
 * would have nowhere to go.
 */
TEST(ProjectCreatorTest, AsksTheTemplateOnlyAfterTheDirectoryExists)
{
    FailingFileSystem files;
    files.createDirectoryError = std::make_error_code(std::errc::file_exists);
    bool called = false;
    const auto recordCall = [&called](std::string_view) {
        called = true;
        return std::vector<TemplateFile>{};
    };

    const auto root = createProject(files, "/work", "hello", recordCall);

    ASSERT_FALSE(root.has_value());
    EXPECT_NE(std::get_if<PathExists>(&root.error()), nullptr);
    EXPECT_FALSE(called);
}

/**
 * A template file that would land outside the project is refused, and the
 * project directory created for it is removed again.
 */
TEST(ProjectCreatorTest, RefusesATemplateFileOutsideTheProject)
{
    for (const char* path : {"../outside.txt", "/tmp/outside.txt", "nested/../../outside.txt"}) {
        FailingFileSystem files;

        const auto root = createProject(files, "/work", "hello", [path](std::string_view) {
            return std::vector<TemplateFile>{TemplateFile{.path = path, .content = "outside\n"}};
        });

        ASSERT_FALSE(root.has_value()) << path;
        const auto* error = std::get_if<CannotCreate>(&root.error());
        ASSERT_NE(error, nullptr) << path;
        EXPECT_EQ(error->code, std::errc::invalid_argument) << path;
        EXPECT_EQ(error->reason, "the template file path leaves the project directory") << path;
        EXPECT_EQ(files.removeCalls, 1) << path;
    }
}

/**
 * The built-in template answers a name it cannot write with no files at all.
 */
TEST(ProjectCreatorTest, DefaultTemplateGivesNoFilesForAnInvalidName)
{
    EXPECT_TRUE(defaultTemplateFiles("a\"b").empty());
    EXPECT_TRUE(defaultTemplateFiles("").empty());
    EXPECT_EQ(defaultTemplateFiles("hello").size(), 2U);
}

/**
 * A write the file system refuses removes the project directory created for
 * it, and the error names the file and the cause.
 */
TEST(ProjectCreatorTest, RemovesTheProjectWhenTheFileSystemRefusesAWrite)
{
    FailingFileSystem files;
    files.writeError = std::make_error_code(std::errc::no_space_on_device);

    const auto root = createProject(files, "/work", "hello", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<CannotCreate>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, std::filesystem::path{"/absolute/work/hello/scrap.toml"});
    EXPECT_EQ(error->code, std::errc::no_space_on_device);
    EXPECT_FALSE(error->leftBehind.has_value());
    EXPECT_EQ(files.removeCalls, 1);
}

/**
 * A removal the file system refuses leaves the directory behind, and the error
 * names it.
 */
TEST(ProjectCreatorTest, NamesTheDirectoryLeftBehindByAFailedRemoval)
{
    FailingFileSystem files;
    files.writeError = std::make_error_code(std::errc::no_space_on_device);
    files.removeError = std::make_error_code(std::errc::permission_denied);

    const auto root = createProject(files, "/work", "hello", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<CannotCreate>(&root.error());
    ASSERT_NE(error, nullptr);
    ASSERT_TRUE(error->leftBehind.has_value());
    EXPECT_EQ(*error->leftBehind, std::filesystem::path{"/absolute/work/hello"});
}

/**
 * A directory the file system refuses to create carries its cause, while an
 * existing one is reported as an existing path.
 */
TEST(ProjectCreatorTest, ReportsWhatTheFileSystemSaysAboutTheDirectory)
{
    FailingFileSystem refused;
    refused.createDirectoryError = std::make_error_code(std::errc::permission_denied);
    FailingFileSystem taken;
    taken.createDirectoryError = std::make_error_code(std::errc::file_exists);

    const auto refusedRoot = createProject(refused, "/work", "hello", defaultTemplateFiles);
    const auto takenRoot = createProject(taken, "/work", "hello", defaultTemplateFiles);

    ASSERT_FALSE(refusedRoot.has_value());
    const auto* error = std::get_if<CannotCreate>(&refusedRoot.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, std::errc::permission_denied);
    EXPECT_EQ(refused.removeCalls, 0);
    ASSERT_FALSE(takenRoot.has_value());
    EXPECT_NE(std::get_if<PathExists>(&takenRoot.error()), nullptr);
}

/**
 * A working directory the file system cannot resolve is reported with the path
 * the project would have taken.
 */
TEST(ProjectCreatorTest, ReportsAWorkingDirectoryThatCannotBeResolved)
{
    FailingFileSystem files;
    files.absoluteError = std::make_error_code(std::errc::no_such_file_or_directory);

    const auto root = createProject(files, "/gone", "hello", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<CannotCreate>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, std::filesystem::path{"/gone/hello"});
    EXPECT_EQ(error->code, std::errc::no_such_file_or_directory);
}

/**
 * A parent directory that does not exist is reported with the path that could
 * not be created.
 */
TEST(ProjectCreatorTest, ReportsAMissingParentAsCannotCreate)
{
    const TempDirectory temp;

    const auto root = createProject(diskFileSystem(), temp.path() / "missing", "hello", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<CannotCreate>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, temp.path() / "missing/hello");
    EXPECT_FALSE(error->reason.empty());
    EXPECT_EQ(error->code, std::errc::no_such_file_or_directory);
}
