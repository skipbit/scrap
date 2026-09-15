#include <gtest/gtest.h>

#include "project/ManifestError.h"
#include "project/ProjectLoader.h"
#include "project/ProjectLocator.h"

#include "ManifestSearch.h"
#include "TempDirectory.h"

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
 */
TEST(ProjectLoaderTest, LoadsTheProjectAtTheStartingDirectory)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, ValidManifest);

    const auto project = loadProject(temp.path());

    ASSERT_TRUE(project.has_value());
    EXPECT_EQ(project->root, temp.path());
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
    EXPECT_EQ(project->root, temp.path());
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
    EXPECT_EQ(project->root, temp.path());
}

/**
 * A path that does not exist is reported, not searched from, even inside a
 * project: walking up from it would load that project under a mistyped name.
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
 * A file is not a directory to search from.
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
 * Reaching the filesystem root without a manifest reports where the search
 * started.
 */
TEST(ProjectLoaderTest, ReportsNoProjectWithTheStartingDirectory)
{
    const TempDirectory temp;
    const auto nested = temp.makeDirectory("a/b");
    if (const auto owner = manifestAbove(temp.path()); owner.has_value()) {
        GTEST_SKIP() << "the temp location is inside the project at " << *owner;
    }

    const auto project = loadProject(nested);

    ASSERT_FALSE(project.has_value());
    const auto* error = std::get_if<ProjectNotFound>(&project.error());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->startDir, nested);
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
    EXPECT_EQ(error->file, temp.path() / ManifestFileName);
    EXPECT_EQ(error->key, "package.name");
}
