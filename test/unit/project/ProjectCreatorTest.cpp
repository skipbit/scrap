#include <gtest/gtest.h>

#include "project/DefaultTemplate.h"
#include "project/Manifest.h"
#include "project/ProjectCreator.h"
#include "project/ProjectLoader.h"
#include "project/ProjectLocator.h"
#include "project/TargetResolver.h"
#include "project/TemplateFile.h"

#include "TempDirectory.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace scrap::Project;
using scrap::TestSupport::TempDirectory;

namespace {

auto readFile(const std::filesystem::path& file) -> std::string
{
    std::ifstream input(file, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

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
 * The project directory is created with every template file in it.
 */
TEST(ProjectCreatorTest, CreatesTheProjectWithItsFiles)
{
    const TempDirectory temp;

    const auto root = createProject(temp.path(), "hello", defaultTemplateFiles);

    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(*root, temp.path() / "hello");
    EXPECT_EQ(readFile(*root / "scrap.toml"), "[package]\nname = \"hello\"\nversion = \"0.1.0\"\nstd = \"23\"\n");
    EXPECT_TRUE(std::filesystem::is_regular_file(*root / "src/main.cpp"));
}

/**
 * A project from the built-in template loads, and its default layout yields
 * one executable named after the project.
 */
TEST(ProjectCreatorTest, DefaultTemplateLoadsAsAProject)
{
    const TempDirectory temp;
    const auto root = createProject(temp.path(), "hello", defaultTemplateFiles);
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

    const auto root = createProject(temp.path(), "hello", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<PathExists>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, temp.path() / "hello");
    EXPECT_EQ(readFile(temp.path() / "hello/marker.txt"), "keep me\n");
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "hello" / ManifestFileName));
}

/**
 * A file where the directory would go is reported the same way.
 */
TEST(ProjectCreatorTest, ReportsAnExistingFileAsExisting)
{
    const TempDirectory temp;
    temp.writeFile("hello", "a file\n");

    const auto root = createProject(temp.path(), "hello", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    EXPECT_NE(std::get_if<PathExists>(&root.error()), nullptr);
    EXPECT_EQ(readFile(temp.path() / "hello"), "a file\n");
}

/**
 * An invalid name creates nothing, not even a directory the name points at.
 */
TEST(ProjectCreatorTest, RejectsAnInvalidNameWithoutCreatingAnything)
{
    const TempDirectory temp;
    temp.makeDirectory("work");

    const auto root = createProject(temp.path() / "work", "../x", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<InvalidProjectName>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->name, "../x");
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "x"));
    EXPECT_TRUE(std::filesystem::is_empty(temp.path() / "work"));
}

/**
 * The template is asked for its files only once the name is valid, so a
 * template never sees a name it would have to escape.
 */
TEST(ProjectCreatorTest, BuildsTemplateFilesOnlyForAValidName)
{
    const TempDirectory temp;
    bool called = false;
    const auto recordCall = [&called](std::string_view) {
        called = true;
        return std::vector<TemplateFile>{};
    };

    const auto rejected = createProject(temp.path(), "a\"b", recordCall);

    EXPECT_FALSE(rejected.has_value());
    EXPECT_FALSE(called);

    const auto created = createProject(temp.path(), "hello", recordCall);

    EXPECT_TRUE(created.has_value());
    EXPECT_TRUE(called);
}

/**
 * A file that cannot be written removes the project directory created for it.
 */
TEST(ProjectCreatorTest, RemovesThePartialProjectWhenAFileCannotBeWritten)
{
    const TempDirectory temp;
    const std::vector<TemplateFile> files{TemplateFile{.path = "a", .content = "a file\n"},
                                          TemplateFile{.path = "a/b", .content = "under a file\n"}};

    const auto root = createProject(temp.path(), "hello", [&files](std::string_view) {
        return files;
    });

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<CannotCreate>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_FALSE(error->reason.empty());
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "hello"));
}

/**
 * A parent directory that does not exist is reported with the path that could
 * not be created.
 */
TEST(ProjectCreatorTest, ReportsAMissingParentAsCannotCreate)
{
    const TempDirectory temp;

    const auto root = createProject(temp.path() / "missing", "hello", defaultTemplateFiles);

    ASSERT_FALSE(root.has_value());
    const auto* error = std::get_if<CannotCreate>(&root.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, temp.path() / "missing/hello");
    EXPECT_FALSE(error->reason.empty());
}
