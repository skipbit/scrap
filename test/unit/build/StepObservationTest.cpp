#include "build/StepObservation.h"

#include "build/BuildStep.h"
#include "build/StepRecord.h"
#include "compile/IncludeDirectives.h"
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
 * it over, and the search goes on.
 */
TEST_F(StepObservationTest, PassesOverADirectory)
{
    std::filesystem::create_directories(_temp.path() / "src/hello.h");
    BuildStep step = compileStep();
    step.searchedDirectories = { "third", "include" };

    const auto record = observeStep(step);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->absent, Paths{ "third/hello.h" });
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
 * A header named, the directory of the file naming it, the directories
 * named for headers, and where the compiler looks for it.
 */
struct SearchCase {
    const char* name;
    scrap::Compile::IncludedName included;
    std::filesystem::path directory;
    Paths searched;
    Paths expected;
};

class SearchPathsTest : public ::testing::TestWithParam<SearchCase> { };

scrap::Compile::IncludedName named(const char* name, const scrap::Compile::IncludeKind kind)
{
    return scrap::Compile::IncludedName{ .name = name, .kind = kind };
}

}  // anonymous namespace

/**
 * The paths are those the compiler looks at, in the order it looks.
 */
TEST_P(SearchPathsTest, ListsWhereTheCompilerLooks)
{
    const SearchCase& search = GetParam();

    EXPECT_EQ(searchPaths(search.included, search.directory, search.searched), search.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    SearchPathsTest,
    ::testing::Values(
        SearchCase{ .name = "Quoted",
                    .included = named("a.h", scrap::Compile::IncludeKind::Quoted),
                    .directory = "src",
                    .searched = { "include", "third" },
                    .expected = { "src/a.h", "include/a.h", "third/a.h" } },
        SearchCase{ .name = "Angled",
                    .included = named("a.h", scrap::Compile::IncludeKind::Angled),
                    .directory = "src",
                    .searched = { "include", "third" },
                    .expected = { "include/a.h", "third/a.h" } },
        SearchCase{ .name = "Next",
                    .included = named("a.h", scrap::Compile::IncludeKind::Next),
                    .directory = "include",
                    .searched = { "include", "third" },
                    .expected = { "include/a.h", "third/a.h" } },
        SearchCase{ .name = "KeepsTheDirectoriesInTheName",
                    .included = named("lib/a.h", scrap::Compile::IncludeKind::Quoted),
                    .directory = "src",
                    .searched = { "include" },
                    .expected = { "src/lib/a.h", "include/lib/a.h" } },
        SearchCase{ .name = "TheRoot",
                    .included = named("a.h", scrap::Compile::IncludeKind::Quoted),
                    .directory = "",
                    .searched = { "include" },
                    .expected = { "a.h", "include/a.h" } },
        SearchCase{ .name = "Normalises",
                    .included = named("../b/./a.h", scrap::Compile::IncludeKind::Quoted),
                    .directory = "src/x",
                    .searched = { "./-inc" },
                    .expected = { "src/b/a.h", "b/a.h" } },
        SearchCase{ .name = "EachOnce",
                    .included = named("a.h", scrap::Compile::IncludeKind::Quoted),
                    .directory = "include",
                    .searched = { "include", "include/." },
                    .expected = { "include/a.h" } },
        SearchCase{ .name = "Absolute",
                    .included = named("/opt/a.h", scrap::Compile::IncludeKind::Quoted),
                    .directory = "src",
                    .searched = { "include" },
                    .expected = { "/opt/a.h" } }),
    [](const ::testing::TestParamInfo<SearchCase>& info) {
        return std::string{ info.param.name };
    });

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
 * Places, paths a file could appear at, and the directories whose entries
 * change when one does.
 */
struct DirectoriesCase {
    const char* name;
    std::set<std::filesystem::path> places;
    Paths paths;
    Paths expected;
};

class ShadowingDirectoriesTest : public ::testing::TestWithParam<DirectoriesCase> { };

}  // anonymous namespace

/**
 * The directories are those of each path and above it, up to the outermost
 * place above it, sorted.
 */
TEST_P(ShadowingDirectoriesTest, ListsTheDirectoriesUpToTheOutermostPlace)
{
    const DirectoriesCase& directories = GetParam();

    EXPECT_EQ(shadowingDirectories(directories.places, directories.paths), directories.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    ShadowingDirectoriesTest,
    ::testing::Values(
        DirectoriesCase{ .name = "NoPaths", .places = { "include", "src" }, .paths = {}, .expected = {} },
        DirectoriesCase{ .name = "DirectlyBelowAPlace", .places = { "include", "src" }, .paths = { "src/a.h" }, .expected = { "src" } },
        DirectoriesCase{ .name = "BelowADirectoryOfAPlace",
                         .places = { "include", "src" },
                         .paths = { "src/lib/a.h" },
                         .expected = { "src", "src/lib" } },
        DirectoriesCase{ .name = "UpToTheOutermostPlace",
                         .places = { "include", "include/lib" },
                         .paths = { "include/lib/a.h" },
                         .expected = { "include", "include/lib" } },
        DirectoriesCase{ .name = "TheRoot",
                         .places = { ".", "include" },
                         .paths = { "a.h", "include/include/a.h" },
                         .expected = { ".", "include", "include/include" } },
        DirectoriesCase{ .name = "Absolute",
                         .places = { "/opt/include", "src" },
                         .paths = { "/opt/include/lib/a.h" },
                         .expected = { "/opt/include", "/opt/include/lib" } }),
    [](const ::testing::TestParamInfo<DirectoriesCase>& info) {
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
