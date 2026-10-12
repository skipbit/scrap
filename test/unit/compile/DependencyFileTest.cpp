#include "compile/DependencyFile.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace scrap::Compile;

using Paths = std::vector<std::filesystem::path>;

/**
 * The files after the target are read in order, across the lines a
 * backslash continues, as GCC 13 wrote them.
 */
TEST(DependencyFileTest, ReadsWhatGccWrote)
{
    const std::string text = "build/debug/obj/hello/src/main.cpp.o: src/main.cpp /usr/include/stdc-predef.h \\\n"
                             " /usr/include/c++/13/iostream \\\n"
                             " include/hello.h\n";

    EXPECT_EQ(parseDependencyFile(text), (Paths{ "src/main.cpp", "/usr/include/stdc-predef.h", "/usr/include/c++/13/iostream", "include/hello.h" }));
}

/**
 * A space or a '#' preceded by a backslash and a doubled '$' are part of a
 * name, as GCC 13 and Clang 22 write a target and headers that hold them; a
 * ':' within a name is kept.
 */
TEST(DependencyFileTest, UndoesTheEscapesInNames)
{
    const std::string gcc = "o\\ g++/x.o: m\\ a.cpp /usr/include/stdc-predef.h in\\ c/sp\\ ace.h \\\n"
                            " h\\#a$$sh.h co:lon.h\n";
    const std::string clang = "o\\ clang++/x.o: m\\ a.cpp in\\ c/sp\\ ace.h h\\#a$$sh.h co:lon.h\n";

    EXPECT_EQ(parseDependencyFile(gcc), (Paths{ "m a.cpp", "/usr/include/stdc-predef.h", "in c/sp ace.h", "h#a$sh.h", "co:lon.h" }));
    EXPECT_EQ(parseDependencyFile(clang), (Paths{ "m a.cpp", "in c/sp ace.h", "h#a$sh.h", "co:lon.h" }));
}

/**
 * A backslash before anything other than a space, a '#' or a line break is
 * part of the name, and a lone '$' too.
 */
TEST(DependencyFileTest, KeepsOtherBackslashesAndDollars)
{
    EXPECT_EQ(parseDependencyFile("x.o: back\\\\slash.h a\\b.h $x.h\n"), (Paths{ "back\\\\slash.h", "a\\b.h", "$x.h" }));
}

/**
 * A rule with nothing after the colon lists no files.
 */
TEST(DependencyFileTest, ReadsARuleWithoutFiles)
{
    EXPECT_EQ(parseDependencyFile("x.o:\n"), Paths{});
    EXPECT_EQ(parseDependencyFile("x.o: \\\n\n"), Paths{});
}

/**
 * What is not a single rule is not read: no target, an empty target, or a
 * second target, as -MP would add.
 */
TEST(DependencyFileTest, RejectsWhatIsNotASingleRule)
{
    const std::vector<std::string> rejected = {
        "",
        "\n",
        "x.o a.cpp\n",
        ": a.cpp\n",
        "x.o : a.cpp\n",
        "x.o: a.cpp a.h\na.h:\n",
    };
    for (const std::string& text : rejected) {
        EXPECT_EQ(parseDependencyFile(text), std::nullopt) << "text: " << text;
    }
}
