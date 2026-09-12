#include <gtest/gtest.h>

#include "project/Manifest.h"
#include "project/TargetResolver.h"

#include "TempDirectory.h"

using namespace scrap::Project;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * A manifest with the package filled in and no targets declared.
 */
Manifest manifestNamed(const char* name)
{
    Manifest manifest;
    manifest.package.name = name;
    manifest.package.version = "0.1.0";
    manifest.package.standard = "23";
    return manifest;
}

}  // namespace

/**
 * Declared targets are used as they stand; the layout is not consulted.
 */
TEST(TargetResolverTest, ReturnsDeclaredTargetsUnchanged)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", "int main() { return 0; }\n");

    Manifest manifest = manifestNamed("my-app");
    manifest.targets.push_back(
        Target{.kind = TargetKind::Library, .name = "declared", .entryPoint = "other/entry.cpp"});

    const auto targets = resolveTargets(temp.path(), manifest);

    ASSERT_EQ(targets.size(), 1);
    EXPECT_EQ(targets[0].kind, TargetKind::Library);
    EXPECT_EQ(targets[0].name, "declared");
    EXPECT_EQ(targets[0].entryPoint, "other/entry.cpp");
}

/**
 * With no targets declared, src/main.cpp becomes one executable named after
 * the package.
 */
TEST(TargetResolverTest, InfersExecutableFromTheDefaultLayout)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", "int main() { return 0; }\n");

    const auto targets = resolveTargets(temp.path(), manifestNamed("my-app"));

    ASSERT_EQ(targets.size(), 1);
    EXPECT_EQ(targets[0].kind, TargetKind::Executable);
    EXPECT_EQ(targets[0].name, "my-app");
    EXPECT_EQ(targets[0].entryPoint, "src/main.cpp");
}

/**
 * Without a declaration and without src/main.cpp there is nothing to build.
 */
TEST(TargetResolverTest, ReturnsNothingWhenTheDefaultEntryPointIsAbsent)
{
    const TempDirectory temp;
    temp.writeFile("src/other.cpp", "int other() { return 0; }\n");

    EXPECT_TRUE(resolveTargets(temp.path(), manifestNamed("my-app")).empty());
}

/**
 * A directory named src/main.cpp is not an entry point.
 */
TEST(TargetResolverTest, IgnoresADirectoryNamedLikeTheEntryPoint)
{
    const TempDirectory temp;
    temp.makeDirectory("src/main.cpp");

    EXPECT_TRUE(resolveTargets(temp.path(), manifestNamed("my-app")).empty());
}
