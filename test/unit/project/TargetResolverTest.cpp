#include "project/TargetResolver.h"

#include "project/LanguageStandard.h"
#include "project/Manifest.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <vector>

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
    manifest.package.standard = LanguageStandard::Cxx23;
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
    manifest.declaresTargets = true;
    manifest.targets.push_back(Target{ .kind = TargetKind::Library, .name = "declared", .source = "other/entry.cpp" });

    const auto targets = resolveTargets(temp.path(), manifest);

    ASSERT_EQ(targets.size(), 1);
    EXPECT_EQ(targets[0].kind, TargetKind::Library);
    EXPECT_EQ(targets[0].name, "declared");
    EXPECT_EQ(targets[0].source, "other/entry.cpp");
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
    EXPECT_EQ(targets[0].source, "src/main.cpp");
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

/**
 * A manifest that declares an empty list of targets builds nothing, and the
 * default layout is not consulted to contradict it.
 */
TEST(TargetResolverTest, DoesNotInferWhenTheDeclarationIsEmpty)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", "int main() { return 0; }\n");

    Manifest manifest = manifestNamed("my-app");
    manifest.declaresTargets = true;

    EXPECT_TRUE(resolveTargets(temp.path(), manifest).empty());
}

/**
 * A declared entry point is returned whether or not the file is there.
 * Inference asks the layout whether a target exists at all, which is a
 * different question from whether a declared source is present; the missing
 * file is the build's to report.
 */
TEST(TargetResolverTest, ReturnsADeclaredEntryPointThatIsNotOnDisk)
{
    const TempDirectory temp;

    Manifest manifest = manifestNamed("my-app");
    manifest.declaresTargets = true;
    manifest.targets.push_back(Target{ .kind = TargetKind::Executable, .name = "my-app", .source = "src/typo.cpp" });

    const auto targets = resolveTargets(temp.path(), manifest);

    ASSERT_EQ(targets.size(), 1);
    EXPECT_EQ(targets[0].source, "src/typo.cpp");
}

/**
 * A build of one target needs that target and none of the others.
 */
TEST(TargetResolverTest, BuildsOnlyTheTargetAskedFor)
{
    const std::vector<Target> targets{
        Target{ .kind = TargetKind::Executable, .name = "app", .source = "src/main.cpp" },
        Target{ .kind = TargetKind::Executable, .name = "tool", .source = "src/tool.cpp" },
    };

    const auto chosen = targetsToBuild(targets, "tool");

    ASSERT_TRUE(chosen.has_value());
    ASSERT_EQ(chosen->size(), 1);
    EXPECT_EQ(chosen->front().name, "tool");
    EXPECT_EQ(chosen->front().source, "src/tool.cpp");
}

/**
 * A build of an executable needs the library of its project as well, and a
 * build of the library needs nothing else.
 */
TEST(TargetResolverTest, BuildsTheLibraryAnExecutableUses)
{
    const std::vector<Target> targets{
        Target{ .kind = TargetKind::Executable, .name = "app", .source = "src/main.cpp" },
        Target{ .kind = TargetKind::Executable, .name = "tool", .source = "src/tool.cpp" },
        Target{ .kind = TargetKind::Library, .name = "core", .source = {} },
    };

    const auto forTool = targetsToBuild(targets, "tool");
    const auto forCore = targetsToBuild(targets, "core");

    ASSERT_TRUE(forTool.has_value());
    ASSERT_EQ(forTool->size(), 2);
    EXPECT_EQ((*forTool)[0].name, "tool");
    EXPECT_EQ((*forTool)[1].name, "core");
    ASSERT_TRUE(forCore.has_value());
    ASSERT_EQ(forCore->size(), 1);
    EXPECT_EQ(forCore->front().name, "core");
}

/**
 * A name no target has is answered with nothing, so the caller can report it
 * rather than build the wrong thing.
 */
TEST(TargetResolverTest, FindsNothingToBuildForANameNoTargetHas)
{
    const std::vector<Target> targets{
        Target{ .kind = TargetKind::Executable, .name = "app", .source = "src/main.cpp" },
    };

    EXPECT_FALSE(targetsToBuild(targets, "ap").has_value());
    EXPECT_FALSE(targetsToBuild(targets, "").has_value());
}
