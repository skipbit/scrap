#include "compile/HeaderSearch.h"

#include "compile/IncludeDirectives.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace scrap::Compile;

namespace {

using Paths = std::vector<std::filesystem::path>;

/**
 * A header named, the directory of the file naming it, the directories
 * named for headers, and where the compiler looks for it.
 */
struct SearchCase {
    const char* name;
    IncludedName included;
    std::filesystem::path directory;
    Paths searched;
    Paths expected;
    bool stopsAtAFile;
};

class HeaderSearchTest : public ::testing::TestWithParam<SearchCase> { };

IncludedName named(const char* name, const IncludeKind kind)
{
    return IncludedName{ .name = name, .kind = kind };
}

}  // anonymous namespace

/**
 * The paths are those the compiler looks at, in the order it looks, and a
 * file found ends the search unless the name is an include_next one.
 */
TEST_P(HeaderSearchTest, ListsWhereTheCompilerLooks)
{
    const SearchCase& search = GetParam();

    const HeaderSearch found = headerSearch(search.included, search.directory, search.searched);

    EXPECT_EQ(found.paths, search.expected);
    EXPECT_EQ(found.stopsAtAFile, search.stopsAtAFile);
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    HeaderSearchTest,
    ::testing::Values(
        SearchCase{ .name = "Quoted",
                    .included = named("a.h", IncludeKind::Quoted),
                    .directory = "src",
                    .searched = { "include", "third" },
                    .expected = { "src/a.h", "include/a.h", "third/a.h" },
                    .stopsAtAFile = true },
        SearchCase{ .name = "Angled",
                    .included = named("a.h", IncludeKind::Angled),
                    .directory = "src",
                    .searched = { "include", "third" },
                    .expected = { "include/a.h", "third/a.h" },
                    .stopsAtAFile = true },
        SearchCase{ .name = "Next",
                    .included = named("a.h", IncludeKind::Next),
                    .directory = "include",
                    .searched = { "include", "third" },
                    .expected = { "include/a.h", "third/a.h" },
                    .stopsAtAFile = false },
        SearchCase{ .name = "KeepsTheDirectoriesInTheName",
                    .included = named("lib/a.h", IncludeKind::Quoted),
                    .directory = "src",
                    .searched = { "include" },
                    .expected = { "src/lib/a.h", "include/lib/a.h" },
                    .stopsAtAFile = true },
        SearchCase{ .name = "TheRoot",
                    .included = named("a.h", IncludeKind::Quoted),
                    .directory = "",
                    .searched = { "include" },
                    .expected = { "a.h", "include/a.h" },
                    .stopsAtAFile = true },
        SearchCase{ .name = "Normalises",
                    .included = named("../b/./a.h", IncludeKind::Quoted),
                    .directory = "src/x",
                    .searched = { "./-inc" },
                    .expected = { "src/b/a.h", "b/a.h" },
                    .stopsAtAFile = true },
        SearchCase{ .name = "EachOnce",
                    .included = named("a.h", IncludeKind::Quoted),
                    .directory = "include",
                    .searched = { "include", "include/." },
                    .expected = { "include/a.h" },
                    .stopsAtAFile = true },
        SearchCase{ .name = "Absolute",
                    .included = named("/opt/a.h", IncludeKind::Quoted),
                    .directory = "src",
                    .searched = { "include" },
                    .expected = { "/opt/a.h" },
                    .stopsAtAFile = true }),
    [](const ::testing::TestParamInfo<SearchCase>& info) {
        return std::string{ info.param.name };
    });
