#include "compile/IncludeDirectives.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace scrap::Compile;

namespace {

using Names = std::vector<IncludedName>;

IncludedName quoted(const char* name)
{
    return IncludedName{ .name = name, .kind = IncludeKind::Quoted };
}

IncludedName angled(const char* name)
{
    return IncludedName{ .name = name, .kind = IncludeKind::Angled };
}

IncludedName next(const char* name)
{
    return IncludedName{ .name = name, .kind = IncludeKind::Next };
}

/**
 * A text and the headers it names.
 */
struct DirectivesCase {
    const char* name;
    const char* text;
    Names names;
    bool computed;
};

class IncludeDirectivesTest : public ::testing::TestWithParam<DirectivesCase> { };

}  // anonymous namespace

/**
 * The names are read in the order written, with how each is looked for, and
 * a name a macro makes is noted.
 */
TEST_P(IncludeDirectivesTest, ReadsTheHeadersNamed)
{
    const DirectivesCase& directives = GetParam();

    EXPECT_EQ(parseIncludeDirectives(directives.text), (IncludeDirectives{ .names = directives.names, .computed = directives.computed }));
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    IncludeDirectivesTest,
    ::testing::Values(
        DirectivesCase{ .name = "Nothing", .text = "int main() { return 0; }\n", .names = {}, .computed = false },
        DirectivesCase{ .name = "QuotedAndAngled",
                        .text = "#include \"a/b.h\"\n#include <vector>\n",
                        .names = { quoted("a/b.h"), angled("vector") },
                        .computed = false },
        DirectivesCase{ .name = "BlanksAround", .text = "  #  include\t<x.h>\n", .names = { angled("x.h") }, .computed = false },
        DirectivesCase{ .name = "NoBlankBeforeTheName", .text = "#include<x.h>\n#include\"y.h\"\n", .names = { angled("x.h"), quoted("y.h") }, .computed = false },
        DirectivesCase{ .name = "CommentsBetween",
                        .text = "/* c */ #/**/include /* d */ \"x.h\"\n",
                        .names = { quoted("x.h") },
                        .computed = false },
        DirectivesCase{ .name = "AfterACommentOverLines", .text = "/*\n*/ #include \"x.h\"\n", .names = { quoted("x.h") }, .computed = false },
        DirectivesCase{ .name = "ContinuedLines",
                        .text = "#inc\\\nlude \\\r\n\"x.h\"\n",
                        .names = { quoted("x.h") },
                        .computed = false },
        DirectivesCase{ .name = "ContinuedLinesWithBlanksAfterTheBackslash",
                        .text = "#inc\\ \t\nlude \"x.h\"\n",
                        .names = { quoted("x.h") },
                        .computed = false },
        DirectivesCase{ .name = "Digraph", .text = "%:include <x.h>\n", .names = { angled("x.h") }, .computed = false },
        DirectivesCase{ .name = "Import", .text = "#import \"x.h\"\n", .names = { quoted("x.h") }, .computed = false },
        DirectivesCase{ .name = "IncludeNext",
                        .text = "#include_next <x.h>\n#include_next \"y.h\"\n",
                        .names = { next("x.h"), next("y.h") },
                        .computed = false },
        DirectivesCase{ .name = "HasInclude",
                        .text = "#if __has_include(<x.h>) && __has_include ( \"y.h\" ) && __has_include_next(<z.h>)\n#endif\n",
                        .names = { angled("x.h"), quoted("y.h"), next("z.h") },
                        .computed = false },
        DirectivesCase{ .name = "HasIncludeNotApplied",
                        .text = "#if defined(__has_include)\n#ifdef __has_include\n#endif\n#endif\nint my__has_include(int);\n",
                        .names = {},
                        .computed = false },
        DirectivesCase{ .name = "InACommentOrABranchOrAString",
                        .text = "// #include \"a.h\"\n#if 0\n#include \"b.h\"\n#endif\nconst char* s = \"#include <c.h>\";\n",
                        .names = { quoted("a.h"), quoted("b.h"), angled("c.h") },
                        .computed = false },
        DirectivesCase{ .name = "OtherDirectives",
                        .text = "#pragma once\n#define INCLUDE 1\n#if INCLUDE\n#endif\n#includes\n",
                        .names = {},
                        .computed = false },
        DirectivesCase{ .name = "MadeByAMacro", .text = "#include HEADER\n#include \"x.h\"\n", .names = { quoted("x.h") }, .computed = true },
        DirectivesCase{ .name = "HasIncludeMadeByAMacro", .text = "#if __has_include(HEADER)\n#endif\n", .names = {}, .computed = true },
        DirectivesCase{ .name = "UnclosedName", .text = "#include \"x.h\n#include <y.h\n", .names = {}, .computed = true }),
    [](const ::testing::TestParamInfo<DirectivesCase>& info) {
        return std::string{ info.param.name };
    });
