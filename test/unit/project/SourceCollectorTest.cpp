#include <gtest/gtest.h>

#include "project/Manifest.h"
#include "project/SourceCollector.h"

#include "TempDirectory.h"

#include <filesystem>
#include <vector>

using namespace scrap::Project;
using scrap::TestSupport::TempDirectory;

namespace {

constexpr const char* SourceText = "int value() { return 0; }\n";

Target executableNamed(const char* name, const char* entryPoint)
{
    return Target{.kind = TargetKind::Executable, .name = name, .entryPoint = entryPoint};
}

std::vector<std::filesystem::path> sourcesOf(const TargetSources& collected)
{
    return collected.sources;
}

}  // namespace

/**
 * Sources anywhere below src/ are compiled, sorted so the list does not depend
 * on the order the file system reports.
 */
TEST(SourceCollectorTest, CollectsEverySourceBelowSrc)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    temp.writeFile("src/util.cpp", SourceText);
    temp.writeFile("src/detail/helper.cpp", SourceText);

    const auto collected = collectSources(temp.path(), {executableNamed("app", "src/main.cpp")});

    ASSERT_EQ(collected.size(), 1);
    EXPECT_EQ(sourcesOf(collected[0]),
              (std::vector<std::filesystem::path>{"src/detail/helper.cpp", "src/main.cpp", "src/util.cpp"}));
}

/**
 * A source is recognised by its extension, and nothing else below src/ is.
 */
TEST(SourceCollectorTest, RecognisesTheAcceptedExtensions)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    temp.writeFile("src/legacy.cc", SourceText);
    temp.writeFile("src/other.cxx", SourceText);
    temp.writeFile("src/interface.h", "#pragma once\n");
    temp.writeFile("src/notes.txt", "not a source\n");

    const auto collected = collectSources(temp.path(), {executableNamed("app", "src/main.cpp")});

    ASSERT_EQ(collected.size(), 1);
    EXPECT_EQ(sourcesOf(collected[0]),
              (std::vector<std::filesystem::path>{"src/legacy.cc", "src/main.cpp", "src/other.cxx"}));
}

/**
 * Two executables share the code beside them and differ in the file that
 * starts each one.
 */
TEST(SourceCollectorTest, ExcludesTheEntryPointOfAnotherTarget)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    temp.writeFile("src/tool.cpp", SourceText);
    temp.writeFile("src/shared.cpp", SourceText);

    const auto collected =
        collectSources(temp.path(), {executableNamed("app", "src/main.cpp"), executableNamed("tool", "src/tool.cpp")});

    ASSERT_EQ(collected.size(), 2);
    EXPECT_EQ(collected[0].target.name, "app");
    EXPECT_EQ(sourcesOf(collected[0]), (std::vector<std::filesystem::path>{"src/main.cpp", "src/shared.cpp"}));
    EXPECT_EQ(collected[1].target.name, "tool");
    EXPECT_EQ(sourcesOf(collected[1]), (std::vector<std::filesystem::path>{"src/shared.cpp", "src/tool.cpp"}));
}

/**
 * A declaration states what to build wherever the file sits, so an entry point
 * the scan does not reach is compiled all the same.
 */
TEST(SourceCollectorTest, KeepsAnEntryPointOutsideTheSourceDirectory)
{
    const TempDirectory temp;
    temp.writeFile("src/util.cpp", SourceText);
    temp.writeFile("app/start.cpp", SourceText);

    const auto collected = collectSources(temp.path(), {executableNamed("app", "app/start.cpp")});

    ASSERT_EQ(collected.size(), 1);
    EXPECT_EQ(sourcesOf(collected[0]), (std::vector<std::filesystem::path>{"app/start.cpp", "src/util.cpp"}));
}

/**
 * A project without src/ still builds the entry point it declared; reporting a
 * source that is not there is the build's to do.
 */
TEST(SourceCollectorTest, ReturnsTheEntryPointWhenThereIsNoSourceDirectory)
{
    const TempDirectory temp;

    const auto collected = collectSources(temp.path(), {executableNamed("app", "src/main.cpp")});

    ASSERT_EQ(collected.size(), 1);
    EXPECT_EQ(sourcesOf(collected[0]), (std::vector<std::filesystem::path>{"src/main.cpp"}));
}

/**
 * Without a target there is nothing to collect, whatever src/ holds.
 */
TEST(SourceCollectorTest, CollectsNothingWithoutATarget)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);

    EXPECT_TRUE(collectSources(temp.path(), {}).empty());
}
