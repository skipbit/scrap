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
 * Where the compiler looks for a header before the place it was found, and
 * nothing is there, the path is listed; the system's places are not.
 */
TEST_F(StepObservationTest, ListsWhereAHeaderCouldAppear)
{
    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, Paths{ "src/hello.h" });
}

/**
 * A path that holds a file ends the search, so it and the paths after it
 * are not listed.
 */
TEST_F(StepObservationTest, StopsAtAPathThatHoldsAFile)
{
    _temp.writeFile("src/hello.h", "int other;\n");

    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, Paths{});
}

/**
 * The places after the one a header was found in are not listed.
 */
TEST_F(StepObservationTest, ListsNothingAfterWhereAHeaderWasFound)
{
    BuildStep step = compileStep();
    step.searchedDirectories = { "include", "third" };

    const auto record = observeStep(step);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, Paths{ "src/hello.h" });
}

/**
 * A directory where a header could be is passed over, as the compiler passes
 * it over, and the search goes on. It holds no file, so it is listed.
 */
TEST_F(StepObservationTest, PassesOverADirectory)
{
    std::filesystem::create_directories(_temp.path() / "src/hello.h");
    BuildStep step = compileStep();
    step.searchedDirectories = { "third", "include" };

    const auto record = observeStep(step);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, (Paths{ "src/hello.h", "third/hello.h" }));
}

/**
 * A file put in place of a directory the search passed over changes the
 * record.
 */
TEST_F(StepObservationTest, SeesAFileInPlaceOfADirectory)
{
    std::filesystem::create_directories(_temp.path() / "src/hello.h");
    const auto before = observeStep(compileStep());
    std::filesystem::remove(_temp.path() / "src/hello.h");
    _temp.writeFile("src/hello.h", "int other;\n");

    const auto after = observeStep(compileStep());

    ASSERT_TRUE(before.has_value());
    ASSERT_TRUE(after.has_value());
    EXPECT_NE(*before, *after);
}

/**
 * A header named in quotes by another header is looked for in the directory
 * of the one that names it first.
 */
TEST_F(StepObservationTest, LooksBesideTheHeaderThatNamesIt)
{
    _temp.writeFile("src/main.cpp", "#include \"lib/a.h\"\n");
    _temp.writeFile("include/lib/a.h", "#include \"b.h\"\n");
    _temp.writeFile("include/b.h", "int b;\n");
    writeDependencyFile("build/obj/main.o: src/main.cpp include/lib/a.h include/b.h\n");

    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, (Paths{ "include/lib/b.h", "src/lib/a.h" }));
}

/**
 * A header of the system named in brackets is looked for in the directories
 * named for headers, where one of the project could appear in its place.
 */
TEST_F(StepObservationTest, ListsWhereAHeaderCouldAppearInPlaceOfOneOfTheSystem)
{
    _temp.writeFile("src/main.cpp", "#include <vector>\n#include \"hello.h\"\n");

    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, (Paths{ "include/vector", "src/hello.h" }));
}

/**
 * A header named with include_next may be found after the place the naming
 * file was found in, which is not known, so the search goes on past a file.
 */
TEST_F(StepObservationTest, LooksEverywhereForAHeaderNamedNext)
{
    _temp.writeFile("src/main.cpp", "#include_next <hello.h>\n");
    BuildStep step = compileStep();
    step.searchedDirectories = { "include", "third" };

    const auto record = observeStep(step);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, (Paths{ "src/hello.h", "third/hello.h" }));
}

/**
 * A source at the project root looks for a quoted header at the root first.
 */
TEST_F(StepObservationTest, SearchesTheRootForASourceThere)
{
    _temp.writeFile("main.cpp", "#include \"hello.h\"\n");
    writeDependencyFile("build/obj/main.o: main.cpp include/hello.h\n");

    const auto record = observeStep(compileStep("main.cpp"));

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, Paths{ "hello.h" });
}

/**
 * When a file names a header by a macro, where a header could appear is
 * listed for every name a file read has below every place: more paths than
 * the compiler would search, never fewer.
 */
TEST_F(StepObservationTest, ListsEveryPlaceWhenAMacroNamesAHeader)
{
    _temp.writeFile("src/main.cpp", "#define HELLO \"hello.h\"\n#include HELLO\n");

    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, (Paths{ "include/main.cpp", "src/hello.h" }));
}

/**
 * When a macro names a header, a path that holds a file does not end the
 * list: every path where nothing is stays listed.
 */
TEST_F(StepObservationTest, ListsPastAFileWhenAMacroNamesAHeader)
{
    _temp.writeFile("src/main.cpp", "#define HELLO \"hello.h\"\n#include HELLO\n");
    _temp.writeFile("include/main.cpp", "int other;\n");

    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, Paths{ "src/hello.h" });
}

/**
 * When a macro names a header, the other names are still looked for as the
 * compiler looks for them, as well as every name below every place.
 */
TEST_F(StepObservationTest, LooksForTheOtherNamesWhenAMacroNamesAHeader)
{
    _temp.writeFile("src/main.cpp", "#if __has_include(\"config.h\")\n#endif\n#define HELLO \"hello.h\"\n#include HELLO\n");

    const auto record = observeStep(compileStep());

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, (Paths{ "include/config.h", "include/main.cpp", "src/config.h", "src/hello.h" }));
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

namespace {

/**
 * A path and the directories above it.
 */
struct AboveCase {
    const char* name;
    std::filesystem::path path;
    Paths expected;
};

class DirectoriesAboveTest : public ::testing::TestWithParam<AboveCase> { };

}  // anonymous namespace

/**
 * The directories are those of the path and each one above it, nearest first.
 */
TEST_P(DirectoriesAboveTest, ListsTheDirectoryAndEachOneAbove)
{
    const AboveCase& above = GetParam();

    EXPECT_EQ(directoriesAbove(above.path), above.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    DirectoriesAboveTest,
    ::testing::Values(AboveCase{ .name = "Nested", .path = "src/lib/a.h", .expected = { "src/lib", "src", "." } },
                      AboveCase{ .name = "AtTheRoot", .path = "a.h", .expected = { "." } },
                      AboveCase{ .name = "LeadingParents", .path = "../x/a.h", .expected = { "../x", "..", "." } },
                      AboveCase{ .name = "Absolute", .path = "/opt/x/a.h", .expected = { "/opt/x", "/opt", "/" } }),
    [](const ::testing::TestParamInfo<AboveCase>& info) {
        return std::string{ info.param.name };
    });

/**
 * A compilation watches the directories where a header named as one it read
 * could appear; a link watches none.
 */
TEST_F(StepObservationTest, WatchesWhereAHeaderCouldAppear)
{
    EXPECT_EQ(watchedDirectories(compileStep()), (Paths{ "include", "src" }));

    BuildStep link = compileStep();
    link.kind = StepKind::Link;
    EXPECT_EQ(watchedDirectories(link), Paths{});
}

/**
 * Without its dependency file, where a compilation watches cannot be told.
 */
TEST_F(StepObservationTest, CannotTellWhereToWatchWithoutADependencyFile)
{
    std::filesystem::remove(_temp.path() / "build/obj/main.o.d");

    EXPECT_FALSE(watchedDirectories(compileStep()).has_value());
}

/**
 * A name that leads out of every place the compiler starts from is watched
 * where it lands.
 */
TEST_F(StepObservationTest, WatchesAboveANameThatLeavesEveryPlace)
{
    _temp.writeFile("src/a/x.cpp", "#include \"../common.h\"\n");
    _temp.writeFile("include/common.h", "int common;\n");
    writeDependencyFile("build/obj/main.o: src/a/x.cpp include/common.h\n");

    EXPECT_EQ(watchedDirectories(compileStep("src/a/x.cpp")), (Paths{ ".", "src" }));
}

/**
 * Where the directory of a path is not there, the nearest one above it is
 * watched instead.
 */
TEST_F(StepObservationTest, WatchesTheNearestDirectoryThatIsThere)
{
    _temp.writeFile("src/main.cpp", "#include \"lib/a.h\"\n");
    _temp.writeFile("include/lib/a.h", "int a;\n");
    writeDependencyFile("build/obj/main.o: src/main.cpp include/lib/a.h\n");

    EXPECT_EQ(watchedDirectories(compileStep()), (Paths{ "include/lib", "src" }));
}

/**
 * When a macro names a header, the directories of every path a header could
 * appear at are watched.
 */
TEST_F(StepObservationTest, WatchesEveryPlaceWhenAMacroNamesAHeader)
{
    _temp.writeFile("src/main.cpp", "#define HELLO \"hello.h\"\n#include HELLO\n");

    EXPECT_EQ(watchedDirectories(compileStep()), (Paths{ "include", "src" }));
}
