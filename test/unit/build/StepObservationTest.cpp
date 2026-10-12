#include "build/StepObservation.h"

#include "build/BuildStep.h"
#include "build/StepRecord.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace scrap::Build;
using scrap::TestSupport::TempDirectory;

namespace {

using Paths = std::vector<std::filesystem::path>;

/**
 * A project with a compiler, a source, a header in include/ and a header of
 * the system, with the dependency file a compilation of the source wrote.
 */
class StepObservationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        _temp.writeFile("cc", "compiler");
        _temp.writeFile("src/main.cpp", "#include \"hello.h\"\n");
        _temp.writeFile("include/hello.h", "int hello;\n");
        _temp.writeFile("system/vector", "// vector\n");
        writeDependencyFile("build/obj/main.o: src/main.cpp include/hello.h " + system().string() + "\n");
    }

    void writeDependencyFile(const std::string& text) const
    {
        _temp.writeFile("build/obj/main.o.d", text);
    }

    [[nodiscard]] std::filesystem::path compiler() const
    {
        return _temp.path() / "cc";
    }

    [[nodiscard]] std::filesystem::path system() const
    {
        return _temp.path() / "system" / "vector";
    }

    [[nodiscard]] BuildStep compileStep(const char* source = "src/main.cpp") const
    {
        return BuildStep{ .kind = StepKind::Compile,
                          .target = "hello",
                          .subject = source,
                          .directory = _temp.path(),
                          .output = "build/obj/main.o",
                          .arguments = { compiler().string(), "-c", source },
                          .inputs = { source },
                          .searchedDirectories = { "include" },
                          .dependencyFile = "build/obj/main.o.d" };
    }

    [[nodiscard]] static Paths pathsOf(const StepRecord& record)
    {
        Paths paths;
        for (const InputStamp& input : record.inputs) {
            paths.push_back(input.path);
        }
        return paths;
    }

    TempDirectory _temp;
};

}  // anonymous namespace

/**
 * A compilation reads its compiler, its source and every file its dependency
 * file lists, each named as the step names it.
 */
TEST_F(StepObservationTest, ObservesWhatACompilationRead)
{
    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->arguments, compileStep().arguments);
    EXPECT_EQ(pathsOf(*record), (Paths{ compiler(), "src/main.cpp", "src/main.cpp", "include/hello.h", system() }));
    EXPECT_EQ(record->inputs[3].size, std::string{ "int hello;\n" }.size());
    EXPECT_NE(record->inputs[3].modified, 0);
}

/**
 * Where a header of the same name would be found in place of one read, and
 * nothing is there, the path is listed; the system's places are not.
 */
TEST_F(StepObservationTest, ListsWhereAHeaderCouldAppear)
{
    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, (Paths{ "include/main.cpp", "src/hello.h" }));
}

/**
 * A path that holds a file is not listed as absent.
 */
TEST_F(StepObservationTest, DoesNotListAPathThatHoldsAFile)
{
    _temp.writeFile("src/hello.h", "int other;\n");

    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, Paths{ "include/main.cpp" });
}

/**
 * A source at the project root searches the root for headers. Every name a
 * header has below some place is looked for below the others, so
 * include/hello.h, named so below the root, is looked for below include/
 * too: more paths than the compiler would search, never fewer.
 */
TEST_F(StepObservationTest, SearchesTheRootForASourceThere)
{
    _temp.writeFile("main.cpp", "#include \"hello.h\"\n");
    writeDependencyFile("build/obj/main.o: main.cpp include/hello.h\n");

    const auto record = observeStep(compileStep("main.cpp"));

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, (Paths{ "hello.h", "include/include/hello.h", "include/main.cpp" }));
}

/**
 * A changed header changes the record.
 */
TEST_F(StepObservationTest, SeesAChangedHeader)
{
    const auto before = observeStep(compileStep());
    _temp.writeFile("include/hello.h", "int hello = 1;\n");

    const auto after = observeStep(compileStep());

    ASSERT_TRUE(before.has_value());
    ASSERT_TRUE(after.has_value());
    EXPECT_NE(*before, *after);
}

/**
 * Whether a compilation needs to run cannot be told without its dependency
 * file, or when a file it read is gone.
 */
TEST_F(StepObservationTest, CannotTellWithoutWhatACompilationRead)
{
    BuildStep withoutDependencyFile = compileStep();
    withoutDependencyFile.dependencyFile.reset();
    EXPECT_FALSE(observeStep(withoutDependencyFile).has_value());

    writeDependencyFile("not a rule\n");
    EXPECT_FALSE(observeStep(compileStep()).has_value());

    writeDependencyFile("build/obj/main.o: src/main.cpp include/gone.h\n");
    EXPECT_FALSE(observeStep(compileStep()).has_value());

    std::filesystem::remove(_temp.path() / "build/obj/main.o.d");
    EXPECT_FALSE(observeStep(compileStep()).has_value());
}

/**
 * Whether a step needs to run cannot be told when its program or one of its
 * inputs is gone.
 */
TEST_F(StepObservationTest, CannotTellWithoutTheProgramOrAnInput)
{
    BuildStep withoutProgram = compileStep();
    withoutProgram.arguments.front() = (_temp.path() / "gone").string();
    EXPECT_FALSE(observeStep(withoutProgram).has_value());

    BuildStep withoutInput = compileStep();
    withoutInput.inputs = { "src/gone.cpp" };
    EXPECT_FALSE(observeStep(withoutInput).has_value());
}

/**
 * A link reads its program and its inputs, and needs no dependency file.
 */
TEST_F(StepObservationTest, ObservesWhatALinkReads)
{
    _temp.writeFile("build/obj/main.o", "object");
    const BuildStep link{ .kind = StepKind::Link,
                          .target = "hello",
                          .subject = "build/bin/hello",
                          .directory = _temp.path(),
                          .output = "build/bin/hello",
                          .arguments = { compiler().string(), "build/obj/main.o", "-o", "build/bin/hello" },
                          .inputs = { "build/obj/main.o" },
                          .searchedDirectories = {},
                          .dependencyFile = std::nullopt };

    const auto record = observeStep(link);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(pathsOf(*record), (Paths{ compiler(), "build/obj/main.o" }));
    EXPECT_TRUE(record->absent.empty());
}

namespace {

/**
 * Places, the files read, and the paths a file could appear at in place of
 * one of them.
 */
struct ShadowingCase {
    const char* name;
    std::set<std::filesystem::path> places;
    Paths files;
    Paths expected;
};

class ShadowingPathsTest : public ::testing::TestWithParam<ShadowingCase> { };

}  // anonymous namespace

/**
 * The paths are the same name below each other place, sorted, whatever is
 * at them.
 */
TEST_P(ShadowingPathsTest, ListsTheSameNameBelowEachOtherPlace)
{
    const ShadowingCase& shadowing = GetParam();

    EXPECT_EQ(shadowingPaths(shadowing.places, shadowing.files), shadowing.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    ShadowingPathsTest,
    ::testing::Values(
        ShadowingCase{ .name = "NoPlaces", .places = {}, .files = { "include/a.h" }, .expected = {} },
        ShadowingCase{ .name = "OnePlace", .places = { "include" }, .files = { "include/a.h" }, .expected = {} },
        ShadowingCase{ .name = "TwoPlaces",
                       .places = { "include", "src" },
                       .files = { "include/a.h" },
                       .expected = { "src/a.h" } },
        ShadowingCase{ .name = "KeepsTheDirectoriesBelowThePlace",
                       .places = { "include", "src" },
                       .files = { "include/lib/a.h" },
                       .expected = { "src/lib/a.h" } },
        ShadowingCase{ .name = "NamedBelowTwoPlaces",
                       .places = { ".", "include" },
                       .files = { "include/a.h" },
                       .expected = { "a.h", "include/include/a.h" } },
        ShadowingCase{ .name = "NormalisesTheFile",
                       .places = { "include", "src" },
                       .files = { "src/../include/./a.h" },
                       .expected = { "src/a.h" } },
        ShadowingCase{ .name = "IgnoresAFileBelowNoPlace",
                       .places = { "include", "src" },
                       .files = { "/usr/include/vector" },
                       .expected = {} },
        ShadowingCase{ .name = "ListsEachPathOnceSorted",
                       .places = { "include", "src", "third" },
                       .files = { "src/b.h", "include/a.h", "third/a.h" },
                       .expected = { "include/a.h", "include/b.h", "src/a.h", "third/a.h", "third/b.h" } }),
    [](const ::testing::TestParamInfo<ShadowingCase>& info) {
        return std::string{ info.param.name };
    });
