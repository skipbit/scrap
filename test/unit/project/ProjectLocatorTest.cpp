#include <gtest/gtest.h>

#include "project/ProjectLocator.h"

#include "TempDirectory.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>

using namespace scrap::Project;
using scrap::TestSupport::TempDirectory;

namespace {

constexpr std::string_view EmptyManifest = "[package]\nname = \"a\"\nversion = \"0.1.0\"\n";

/**
 * The first directory at or above @p directory that holds a manifest.
 *
 * The "no project anywhere above" tests depend on the machine's temp location
 * not sitting inside a scrap project. Checking it makes an unusual machine
 * skip the test, naming the directory responsible, instead of reporting a
 * failure that is not about the code.
 *
 * The predicate has to be the one findProjectRoot uses. Testing existence
 * instead would treat a *directory* named scrap.toml as a project and skip
 * the very test that exists to show it is not one.
 */
std::optional<std::filesystem::path> manifestAbove(const std::filesystem::path& directory)
{
    std::error_code ec;
    for (std::filesystem::path current = directory;; current = current.parent_path()) {
        if (std::filesystem::is_regular_file(current / ManifestFileName, ec)) {
            return current;
        }
        if (current.parent_path() == current) {
            return std::nullopt;
        }
    }
}

}  // namespace

/**
 * A directory holding a manifest is itself the project root.
 */
TEST(ProjectLocatorTest, FindsManifestInTheStartingDirectory)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, EmptyManifest);

    const auto root = findProjectRoot(temp.path());

    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(*root, temp.path());
}

/**
 * A directory below the project root still resolves to the project.
 */
TEST(ProjectLocatorTest, WalksUpFromASubdirectory)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, EmptyManifest);
    const auto nested = temp.makeDirectory("src/detail/deep");

    const auto root = findProjectRoot(nested);

    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(*root, temp.path());
}

/**
 * The nearest manifest wins, so a project nested inside another resolves to
 * the inner one.
 */
TEST(ProjectLocatorTest, StopsAtTheNearestManifest)
{
    const TempDirectory temp;
    temp.writeFile(ManifestFileName, EmptyManifest);
    const auto inner = temp.makeDirectory("vendor/inner");
    temp.writeFile("vendor/inner/scrap.toml", EmptyManifest);

    const auto root = findProjectRoot(inner);

    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(*root, inner);
}

/**
 * Reaching the filesystem root without a manifest means there is no project.
 */
TEST(ProjectLocatorTest, ReturnsNothingWhenNoManifestExistsAbove)
{
    const TempDirectory temp;
    const auto nested = temp.makeDirectory("a/b");
    if (const auto owner = manifestAbove(temp.path()); owner.has_value()) {
        GTEST_SKIP() << "the temp location is inside the project at " << *owner;
    }

    EXPECT_FALSE(findProjectRoot(nested).has_value());
}

/**
 * A directory that merely shares the manifest's name is not a project root.
 */
TEST(ProjectLocatorTest, IgnoresADirectoryNamedLikeTheManifest)
{
    const TempDirectory temp;
    const auto decoy = temp.makeDirectory("decoy");
    temp.makeDirectory("decoy/scrap.toml");
    if (const auto owner = manifestAbove(temp.path()); owner.has_value()) {
        GTEST_SKIP() << "the temp location is inside the project at " << *owner;
    }

    EXPECT_FALSE(findProjectRoot(decoy).has_value());
}
