#include "project/ProjectLoader.h"

#include "ManifestSearch.h"
#include "project/ManifestError.h"
#include "project/ProjectLocator.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>
#include <variant>

using namespace scrap::Project;
using scrap::TestSupport::manifestAbove;
using scrap::TestSupport::TempDirectory;

namespace {

constexpr std::string_view ValidManifest = "[package]\nname = \"app\"\nversion = \"0.1.0\"\n";

}  // namespace

/**
 * A directory holding a manifest loads as the project, manifest included.
 * The root is canonical, which on some systems differs from the temp path.
 */
TEST(ProjectLoaderTest, LoadsTheProjectAtTheStartingDirectory)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, ValidManifest);

    const auto project = loadProject(temp.path());

    ASSERT_TRUE(project.has_value());
    EXPECT_EQ(project->root, std::filesystem::canonical(temp.path()));
    EXPECT_EQ(project->manifest.package.name, "app");
}

/**
 * A directory below the project root loads the enclosing project.
 */
TEST(ProjectLoaderTest, LoadsTheProjectAboveASubdirectory)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, ValidManifest);
    const auto nested = temp.makeDirectory("src/detail");

    const auto project = loadProject(nested);

    ASSERT_TRUE(project.has_value());
    EXPECT_EQ(project->root, std::filesystem::canonical(temp.path()));
}

/**
 * A trailing separator names the same directory, so the root carries none.
 */
TEST(ProjectLoaderTest, DropsATrailingSeparator)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, ValidManifest);

    const auto project = loadProject(temp.path() / "");

    ASSERT_TRUE(project.has_value());
    EXPECT_EQ(project->root, std::filesystem::canonical(temp.path()));
}

/**
 * A symbolic link is followed before walking up, so the search climbs the
 * directories the link points into rather than the ones it sits in.
 */
TEST(ProjectLoaderTest, FollowsASymbolicLinkBeforeWalkingUp)
{
    const TempDirectory temp;
    temp.writeFile("real/scrap.toml", ValidManifest);
    temp.makeDirectory("real/sub");
    std::filesystem::create_symlink(temp.path() / "real/sub", temp.path() / "link");

    const auto project = loadProject(temp.path() / "link");

    ASSERT_TRUE(project.has_value());
    EXPECT_EQ(project->root, std::filesystem::canonical(temp.path() / "real"));
}

/**
 * A path that does not exist is reported inside a project too, where walking
 * up from it would load that project under a mistyped name.
 */
TEST(ProjectLoaderTest, ReportsAMissingPathEvenInsideAProject)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, ValidManifest);

    const auto project = loadProject(temp.path() / "missing");

    ASSERT_FALSE(project.has_value());
    const auto* error = std::get_if<NotADirectory>(&project.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, temp.path() / "missing");
}

/**
 * ".." after a missing entry fails to resolve, as it does in a shell, and is
 * reported as written.
 */
TEST(ProjectLoaderTest, ReportsAPathThroughAMissingEntry)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, ValidManifest);

    const auto project = loadProject(temp.path() / "missing/..");

    ASSERT_FALSE(project.has_value());
    const auto* error = std::get_if<NotADirectory>(&project.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, temp.path() / "missing/..");
}

/**
 * ".." after a file fails to resolve as well.
 */
TEST(ProjectLoaderTest, ReportsAPathThroughAFile)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, ValidManifest);

    const auto project = loadProject(temp.path() / "scrap.toml/..");

    ASSERT_FALSE(project.has_value());
    EXPECT_NE(std::get_if<NotADirectory>(&project.error()), nullptr);
}

/**
 * A file is reported as not a directory.
 */
TEST(ProjectLoaderTest, ReportsAFileAsNotADirectory)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, ValidManifest);
    const auto file = temp.writeFile("src/main.cpp", "int main() {}\n");

    const auto project = loadProject(file);

    ASSERT_FALSE(project.has_value());
    const auto* error = std::get_if<NotADirectory>(&project.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, file);
}

/**
 * A path the filesystem cannot examine is reported with the system's reason,
 * distinct from a path that is not a directory.
 */
TEST(ProjectLoaderTest, ReportsASymbolicLinkLoopAsInaccessible)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, ValidManifest);
    std::filesystem::create_symlink("loop-b", temp.path() / "loop-a");
    std::filesystem::create_symlink("loop-a", temp.path() / "loop-b");

    const auto project = loadProject(temp.path() / "loop-a");

    ASSERT_FALSE(project.has_value());
    const auto* error = std::get_if<PathInaccessible>(&project.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->path, temp.path() / "loop-a");
    EXPECT_FALSE(error->reason.empty());
}

/**
 * Reaching the filesystem root without a manifest reports where the search
 * started.
 */
TEST(ProjectLoaderTest, ReportsNoProjectWithTheStartingDirectory)
{
    const TempDirectory temp;
    const auto nested = temp.makeDirectory("a/b");
    if (const auto owner = manifestAbove(std::filesystem::canonical(temp.path())); owner.has_value()) {
        GTEST_SKIP() << "the temp location is inside the project at " << *owner;
    }

    const auto project = loadProject(nested);

    ASSERT_FALSE(project.has_value());
    const auto* error = std::get_if<ProjectNotFound>(&project.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->startDir, std::filesystem::canonical(nested));
}

/**
 * A manifest that does not parse is reported with the parser's error.
 */
TEST(ProjectLoaderTest, ReportsTheManifestError)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, "[package]\nversion = \"0.1.0\"\n");

    const auto project = loadProject(temp.path());

    ASSERT_FALSE(project.has_value());
    const auto* error = std::get_if<ManifestError>(&project.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->file, std::filesystem::canonical(temp.path()) / ManifestFileName);
    EXPECT_EQ(error->key, "package.name");
    EXPECT_EQ(error->kind, ManifestErrorKind::Invalid);
}
