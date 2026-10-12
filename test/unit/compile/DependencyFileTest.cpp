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
 * A tab is escaped as a space is, and the backslashes before either are
 * doubled with one added, as GCC 13 writes a header named "ta<tab>b.h", one
 * named "bs\ sp.h" and one named "bs2\\ sp.h". An even count of them ends
 * the name after half.
 */
TEST(DependencyFileTest, UndoesTheEscapesBeforeABlank)
{
    const std::string gcc = "x.o: m.cpp ta\\\tb.h bs\\\\\\ sp.h \\\n"
                            " bs2\\\\\\\\\\ sp.h\n";

    EXPECT_EQ(parseDependencyFile(gcc), (Paths{ "m.cpp", "ta\tb.h", "bs\\ sp.h", "bs2\\\\ sp.h" }));
    EXPECT_EQ(parseDependencyFile("x.o: a\\\\ b.h\n"), (Paths{ "a\\", "b.h" }));
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
 * The rules -MP adds after the first, a header with nothing after its colon,
 * are passed over, as GCC 13 and Clang 22 write them.
 */
TEST(DependencyFileTest, PassesOverTheRulesMpAdds)
{
    const std::string gcc = "x.o: m\\ a.cpp /usr/include/stdc-predef.h h\\#a$$sh.h \\\n"
                            " co:lon.h\n"
                            "/usr/include/stdc-predef.h:\n"
                            "h\\#a$$sh.h:\n"
                            "co:lon.h:\n";
    const std::string clang = "x.o: m\\ a.cpp h\\#a$$sh.h co:lon.h\n"
                              "h\\#a$$sh.h:\n"
                              "co:lon.h:\n";

    EXPECT_EQ(parseDependencyFile(gcc), (Paths{ "m a.cpp", "/usr/include/stdc-predef.h", "h#a$sh.h", "co:lon.h" }));
    EXPECT_EQ(parseDependencyFile(clang), (Paths{ "m a.cpp", "h#a$sh.h", "co:lon.h" }));
}

/**
 * What is not a rule followed only by rules without files is not read.
 */
TEST(DependencyFileTest, RejectsWhatIsNotASingleRule)
{
    struct Rejected {
        const char* reason;
        std::string text;
    };
    const std::vector<Rejected> rejected = {
        { "empty", "" },
        { "only a line break", "\n" },
        { "no colon", "x.o a.cpp\n" },
        { "empty target", ": a.cpp\n" },
        { "colon apart from the target", "x.o : a.cpp\n" },
        { "second rule with files", "x.o: a.cpp\ny.o: b.cpp\n" },
        { "added rule with a file", "x.o: a.cpp a.h\na.h: b.h\n" },
        { "empty target after the rule", "x.o: a.cpp\n:\n" },
    };
    for (const Rejected& each : rejected) {
        EXPECT_EQ(parseDependencyFile(each.text), std::nullopt) << "reason: " << each.reason << "\ntext: " << each.text;
    }
}
