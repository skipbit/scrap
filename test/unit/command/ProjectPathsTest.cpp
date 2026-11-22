#include "command/ProjectPaths.h"

#include "command/ProjectArgument.h"
#include "project/FileSelection.h"
#include "project/ProjectLoader.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

using namespace scrap::Command;
using scrap::Project::RequestedPath;
using scrap::TestSupport::TempDirectory;

namespace {

constexpr std::string_view Manifest = "[package]\nname = \"app\"\nversion = \"0.1.0\"\n";

/**
 * Write a project at "app" in @p temp, with src/main.cpp and src/net/b.cpp.
 */
void writeProject(const TempDirectory& temp)
{
    temp.writeFile("app/scrap.toml", Manifest);
    temp.writeFile("app/src/main.cpp", "int main() {}\n");
    temp.writeFile("app/src/net/b.cpp", "\n");
}

/// The paths of @p requested, in order.
std::vector<std::filesystem::path> pathsOf(const std::vector<RequestedPath>& requested)
{
    std::vector<std::filesystem::path> paths;
    for (const RequestedPath& path : requested) {
        paths.push_back(path.path);
    }
    return paths;
}

}  // namespace

TEST(ProjectPathsTest, SearchesFromTheWorkingDirectoryWithoutPaths)
{
    const TempDirectory temp;
    writeProject(temp);

    const auto loaded = loadProjectWithPaths(temp.path() / "app" / "src", {});

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->project.root, std::filesystem::canonical(temp.path() / "app"));
    EXPECT_TRUE(loaded->requested.empty());
}

/**
 * A file is searched from the directory holding it, and named relative to
 * the project root.
 */
TEST(ProjectPathsTest, SearchesFromTheDirectoryOfAFile)
{
    const TempDirectory temp;
    writeProject(temp);

    const auto loaded = loadProjectWithPaths(temp.path(), { "app/src/main.cpp" });

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->project.root, std::filesystem::canonical(temp.path() / "app"));
    ASSERT_EQ(loaded->requested.size(), 1U);
    EXPECT_EQ(loaded->requested.front().path, "src/main.cpp");
    EXPECT_FALSE(loaded->requested.front().isDirectory);
}

TEST(ProjectPathsTest, NamesDirectoriesAndTheRootInTheOrderGiven)
{
    const TempDirectory temp;
    writeProject(temp);

    const auto loaded = loadProjectWithPaths(temp.path() / "app", { "src/net", "src/main.cpp", "." });

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(pathsOf(loaded->requested), (std::vector<std::filesystem::path>{ "src/net", "src/main.cpp", "." }));
    EXPECT_TRUE(loaded->requested[0].isDirectory);
    EXPECT_FALSE(loaded->requested[1].isDirectory);
    EXPECT_TRUE(loaded->requested[2].isDirectory);
}

/**
 * A path that names nothing is reported as given, whichever place it holds.
 */
TEST(ProjectPathsTest, ReportsAPathThatNamesNothingAsGiven)
{
    const TempDirectory temp;
    writeProject(temp);

    const auto first = loadProjectWithPaths(temp.path() / "app", { "src/nope.cpp", "src/main.cpp" });
    const auto later = loadProjectWithPaths(temp.path() / "app", { "src/main.cpp", "nope/../src" });

    ASSERT_FALSE(first.has_value());
    ASSERT_TRUE(std::holds_alternative<PathMissing>(first.error()));
    EXPECT_EQ(std::get<PathMissing>(first.error()).path, "src/nope.cpp");
    ASSERT_FALSE(later.has_value());
    ASSERT_TRUE(std::holds_alternative<PathMissing>(later.error()));
    EXPECT_EQ(std::get<PathMissing>(later.error()).path, "nope/../src");
}

TEST(ProjectPathsTest, ReportsAnEmptyPath)
{
    const TempDirectory temp;
    writeProject(temp);

    const auto loaded = loadProjectWithPaths(temp.path() / "app", { "src", "" });

    ASSERT_FALSE(loaded.has_value());
    EXPECT_TRUE(std::holds_alternative<EmptyPathArgument>(loaded.error()));
}

TEST(ProjectPathsTest, ReportsAProjectThatCannotBeFound)
{
    const TempDirectory temp;
    writeProject(temp);
    temp.writeFile("loose/a.cpp", "\n");

    const auto loaded = loadProjectWithPaths(temp.path(), { "loose/a.cpp" });

    if (loaded.has_value()) {
        GTEST_SKIP() << "a manifest lies above the temporary directory";
    }
    ASSERT_TRUE(std::holds_alternative<scrap::Project::ProjectError>(loaded.error()));
    EXPECT_TRUE(std::holds_alternative<scrap::Project::ProjectNotFound>(std::get<scrap::Project::ProjectError>(loaded.error())));
}

/**
 * A path in a project nested below the first one's root belongs to that
 * other project.
 */
TEST(ProjectPathsTest, ReportsAPathInANestedProject)
{
    const TempDirectory temp;
    writeProject(temp);
    temp.writeFile("app/tools/scrap.toml", Manifest);
    temp.writeFile("app/tools/src/main.cpp", "\n");

    const auto loaded = loadProjectWithPaths(temp.path() / "app", { "src", "tools/src/main.cpp" });

    ASSERT_FALSE(loaded.has_value());
    ASSERT_TRUE(std::holds_alternative<PathOutsideProject>(loaded.error()));
    const auto& outside = std::get<PathOutsideProject>(loaded.error());
    EXPECT_EQ(outside.path, "tools/src/main.cpp");
    EXPECT_EQ(outside.projectRoot, std::filesystem::canonical(temp.path() / "app"));
}

TEST(ProjectPathsTest, ReportsAPathOutsideTheProject)
{
    const TempDirectory temp;
    writeProject(temp);
    temp.writeFile("loose/a.cpp", "\n");

    const auto loaded = loadProjectWithPaths(temp.path(), { "app/src", "loose/a.cpp" });

    ASSERT_FALSE(loaded.has_value());
    ASSERT_TRUE(std::holds_alternative<PathOutsideProject>(loaded.error()));
    EXPECT_EQ(std::get<PathOutsideProject>(loaded.error()).path, "loose/a.cpp");
}

/**
 * A symbolic link names the file it reaches.
 */
TEST(ProjectPathsTest, NamesTheFileALinkReaches)
{
    const TempDirectory temp;
    writeProject(temp);
    std::error_code code;
    std::filesystem::create_symlink("main.cpp", temp.path() / "app" / "src" / "link.cpp", code);
    if (code) {
        GTEST_SKIP() << "the file system cannot hold a symbolic link: " << code.message();
    }

    const auto loaded = loadProjectWithPaths(temp.path() / "app", { "src/link.cpp" });

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(pathsOf(loaded->requested), (std::vector<std::filesystem::path>{ "src/main.cpp" }));
}

/**
 * On a file system that ignores letter case, a path spelled in another case
 * names the file in the spelling it has.
 */
TEST(ProjectPathsTest, NamesAFileInItsOwnSpellingWhereCaseIsIgnored)
{
    const TempDirectory temp;
    writeProject(temp);
    if (! std::filesystem::exists(temp.path() / "app" / "SRC" / "Main.cpp")) {
        GTEST_SKIP() << "case-sensitive file system";
    }

    const auto loaded = loadProjectWithPaths(temp.path() / "app", { "SRC/Main.cpp" });

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(pathsOf(loaded->requested), (std::vector<std::filesystem::path>{ "src/main.cpp" }));
}

/**
 * A path that goes on below a file names nothing either.
 */
TEST(ProjectPathsTest, ReportsAPathBelowAFileAsNamingNothing)
{
    const TempDirectory temp;
    writeProject(temp);

    const auto loaded = loadProjectWithPaths(temp.path() / "app", { "src/main.cpp/x" });

    ASSERT_FALSE(loaded.has_value());
    EXPECT_TRUE(std::holds_alternative<PathMissing>(loaded.error()));
}

/**
 * A path that cannot be examined is reported as given, as one that names
 * nothing is.
 */
TEST(ProjectPathsTest, ReportsAPathThatCannotBeExaminedAsGiven)
{
    const TempDirectory temp;
    writeProject(temp);
    const std::filesystem::path locked = temp.makeDirectory("app/locked");
    temp.writeFile("app/locked/a.cpp", "\n");
    std::error_code code;
    std::filesystem::permissions(locked, std::filesystem::perms::none, code);
    ASSERT_FALSE(code) << code.message();

    // A user who reads the directory anyway, root among them, cannot observe
    // the failure; the fixture is restored before skipping so it can be removed.
    const bool examined = std::filesystem::exists(locked / "a.cpp", code);
    if (examined && (! code)) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all, code);
        GTEST_SKIP() << "this user reads a directory with no permissions";
    }

    const auto loaded = loadProjectWithPaths(temp.path() / "app", { "locked/a.cpp" });

    std::filesystem::permissions(locked, std::filesystem::perms::owner_all, code);

    ASSERT_FALSE(loaded.has_value());
    ASSERT_TRUE(std::holds_alternative<scrap::Project::ProjectError>(loaded.error()));
    const auto& error = std::get<scrap::Project::ProjectError>(loaded.error());
    ASSERT_TRUE(std::holds_alternative<scrap::Project::PathInaccessible>(error));
    EXPECT_EQ(std::get<scrap::Project::PathInaccessible>(error).path, "locked/a.cpp");
}
