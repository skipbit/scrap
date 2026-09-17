#include <gtest/gtest.h>

#include "project/Manifest.h"
#include "project/SourceCollector.h"

#include "TempDirectory.h"

#include <filesystem>
#include <system_error>
#include <vector>

using namespace scrap::Project;
using scrap::TestSupport::TempDirectory;

namespace {

constexpr const char* SourceText = "int value() { return 0; }\n";

Target executableNamed(const char* name, const char* entryPoint)
{
    return Target{.kind = TargetKind::Executable, .name = name, .entryPoint = entryPoint};
}

Target libraryNamed(const char* name, const char* entryPoint)
{
    return Target{.kind = TargetKind::Library, .name = name, .entryPoint = entryPoint};
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

    ASSERT_TRUE(collected.has_value());
    ASSERT_EQ(collected->size(), 1);
    EXPECT_EQ((*collected)[0].sources,
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

    ASSERT_TRUE(collected.has_value());
    ASSERT_EQ(collected->size(), 1);
    EXPECT_EQ((*collected)[0].sources,
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

    ASSERT_TRUE(collected.has_value());
    ASSERT_EQ(collected->size(), 2);
    EXPECT_EQ((*collected)[0].target.name, "app");
    EXPECT_EQ((*collected)[0].sources, (std::vector<std::filesystem::path>{"src/main.cpp", "src/shared.cpp"}));
    EXPECT_EQ((*collected)[1].target.name, "tool");
    EXPECT_EQ((*collected)[1].sources, (std::vector<std::filesystem::path>{"src/shared.cpp", "src/tool.cpp"}));
}

/**
 * An entry point written with a "." component names the file the scan found,
 * so it is compiled once rather than added a second time.
 */
TEST(SourceCollectorTest, MatchesAnEntryPointWrittenWithADotComponent)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    temp.writeFile("src/util.cpp", SourceText);

    const auto collected = collectSources(temp.path(), {executableNamed("app", "./src/main.cpp")});

    ASSERT_TRUE(collected.has_value());
    ASSERT_EQ(collected->size(), 1);
    EXPECT_EQ((*collected)[0].sources, (std::vector<std::filesystem::path>{"src/main.cpp", "src/util.cpp"}));
}

/**
 * The same spelling is recognised when another target declared it, so one
 * executable's entry point stays out of the other's sources.
 */
TEST(SourceCollectorTest, ExcludesAnEntryPointWrittenWithADotComponent)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    temp.writeFile("src/tool.cpp", SourceText);

    const auto collected = collectSources(
        temp.path(), {executableNamed("app", "./src/main.cpp"), executableNamed("tool", "src/tool.cpp")});

    ASSERT_TRUE(collected.has_value());
    ASSERT_EQ(collected->size(), 2);
    EXPECT_EQ((*collected)[0].sources, (std::vector<std::filesystem::path>{"src/main.cpp"}));
    EXPECT_EQ((*collected)[1].sources, (std::vector<std::filesystem::path>{"src/tool.cpp"}));
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

    ASSERT_TRUE(collected.has_value());
    ASSERT_EQ(collected->size(), 1);
    EXPECT_EQ((*collected)[0].sources, (std::vector<std::filesystem::path>{"app/start.cpp", "src/util.cpp"}));
}

/**
 * A project without src/ still builds the entry point it declared; reporting a
 * source that is not there is the build's to do.
 */
TEST(SourceCollectorTest, ReturnsTheEntryPointWhenThereIsNoSourceDirectory)
{
    const TempDirectory temp;

    const auto collected = collectSources(temp.path(), {executableNamed("app", "src/main.cpp")});

    ASSERT_TRUE(collected.has_value());
    ASSERT_EQ(collected->size(), 1);
    EXPECT_EQ((*collected)[0].sources, (std::vector<std::filesystem::path>{"src/main.cpp"}));
}

/**
 * A directory that exists and cannot be read is reported. Leaving its sources
 * out would link an artifact from fewer files than the project holds, and
 * nothing later would name what went missing.
 */
TEST(SourceCollectorTest, ReportsASourceDirectoryItCannotRead)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    const std::filesystem::path locked = temp.makeDirectory("src/locked");
    temp.writeFile("src/locked/hidden.cpp", SourceText);

    std::error_code ec;
    std::filesystem::permissions(locked, std::filesystem::perms::none, ec);
    ASSERT_FALSE(ec) << ec.message();

    // A user who reads the directory anyway, root among them, cannot observe
    // the failure; the fixture is restored before skipping so it can be removed.
    const std::filesystem::directory_iterator probe(locked, ec);
    if (! ec) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);
        GTEST_SKIP() << "this user reads a directory with no permissions";
    }

    const auto collected = collectSources(temp.path(), {executableNamed("app", "src/main.cpp")});

    std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);

    ASSERT_FALSE(collected.has_value());
    EXPECT_EQ(collected.error().directory, locked);
    EXPECT_FALSE(collected.error().reason.empty());
}

/**
 * A directory whose state cannot be determined at all, here through a link
 * that points at itself, is reported rather than read as one that is absent.
 */
TEST(SourceCollectorTest, ReportsASourceDirectoryItCannotExamine)
{
    const TempDirectory temp;
    std::filesystem::create_directory_symlink("src", temp.path() / "src");

    const auto collected = collectSources(temp.path(), {executableNamed("app", "src/main.cpp")});

    ASSERT_FALSE(collected.has_value());
    EXPECT_EQ(collected.error().directory, temp.path() / "src");
    EXPECT_FALSE(collected.error().reason.empty());
}

/**
 * Targets of different kinds are not separated yet: an executable beside a
 * library is given the library's sources except its entry point. Pinned here
 * so that separating them is a deliberate change rather than a silent one.
 */
TEST(SourceCollectorTest, GivesAnExecutableTheSourcesBesideALibrary)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    temp.writeFile("src/core.cpp", SourceText);
    temp.writeFile("src/detail.cpp", SourceText);

    const auto collected =
        collectSources(temp.path(), {executableNamed("app", "src/main.cpp"), libraryNamed("core", "src/core.cpp")});

    ASSERT_TRUE(collected.has_value());
    ASSERT_EQ(collected->size(), 2);
    EXPECT_EQ((*collected)[0].sources, (std::vector<std::filesystem::path>{"src/detail.cpp", "src/main.cpp"}));
    EXPECT_EQ((*collected)[1].sources, (std::vector<std::filesystem::path>{"src/core.cpp", "src/detail.cpp"}));
}

/**
 * Without a target there is nothing to collect, whatever src/ holds.
 */
TEST(SourceCollectorTest, CollectsNothingWithoutATarget)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);

    const auto collected = collectSources(temp.path(), {});

    ASSERT_TRUE(collected.has_value());
    EXPECT_TRUE(collected->empty());
}
