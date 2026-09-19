#include <gtest/gtest.h>

#include "compile/CompilationDatabase.h"
#include "compile/CompileCommand.h"

#include "support/TempDirectory.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using namespace scrap::Compile;
using scrap::TestSupport::TempDirectory;

namespace {

CompileCommand commandFor(const char* source, const char* output)
{
    return CompileCommand{.directory = "/home/me/hello",
                          .file = source,
                          .output = output,
                          .arguments = {"/usr/bin/c++", "-c", source, "-o", output}};
}

/**
 * The names of the entries in @p directory, which shows whether a staged file
 * was left beside the database.
 */
std::vector<std::string> entriesOf(const std::filesystem::path& directory)
{
    std::vector<std::string> names;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
        names.push_back(entry.path().filename().string());
    }
    return names;
}

}  // namespace

/**
 * A project that compiles nothing still gets a database, one that lists no
 * command.
 */
TEST(CompilationDatabaseTest, RendersNoCommandAsAnEmptyArray)
{
    EXPECT_EQ(renderCompilationDatabase({}), "[]\n");
}

/**
 * An entry carries the directory, the source, each argument on its own and
 * the object file.
 */
TEST(CompilationDatabaseTest, RendersEachFieldOfACommand)
{
    EXPECT_EQ(renderCompilationDatabase({commandFor("src/main.cpp", "build/debug/obj/hello/src/main.cpp.o")}),
              "[\n"
              "  {\n"
              "    \"directory\": \"/home/me/hello\",\n"
              "    \"file\": \"src/main.cpp\",\n"
              "    \"arguments\": [\"/usr/bin/c++\", \"-c\", \"src/main.cpp\", \"-o\", "
              "\"build/debug/obj/hello/src/main.cpp.o\"],\n"
              "    \"output\": \"build/debug/obj/hello/src/main.cpp.o\"\n"
              "  }\n"
              "]\n");
}

/**
 * Entries follow the order of the commands, separated by commas.
 */
TEST(CompilationDatabaseTest, RendersCommandsInTheirOrder)
{
    const std::string rendered =
        renderCompilationDatabase({commandFor("src/b.cpp", "build/debug/obj/hello/src/b.cpp.o"),
                                   commandFor("src/a.cpp", "build/debug/obj/hello/src/a.cpp.o")});

    const auto first = rendered.find("\"file\": \"src/b.cpp\"");
    const auto second = rendered.find("\"file\": \"src/a.cpp\"");
    ASSERT_NE(first, std::string::npos) << rendered;
    ASSERT_NE(second, std::string::npos) << rendered;
    EXPECT_LT(first, second);
    EXPECT_NE(rendered.find("\n  },\n  {\n"), std::string::npos) << rendered;
}

/**
 * A quote, a backslash and a control character in a path are escaped, and
 * letters outside ASCII are kept as they are.
 */
TEST(CompilationDatabaseTest, EscapesWhatJsonRequires)
{
    const CompileCommand command{.directory = "/home/me/\xe3\x81\x82 \"q\"",
                                 .file = "src/back\\slash.cpp",
                                 .output = "o\nline\x01.o",
                                 .arguments = {}};

    const std::string rendered = renderCompilationDatabase({command});

    EXPECT_NE(rendered.find("\"directory\": \"/home/me/\xe3\x81\x82 \\\"q\\\"\""), std::string::npos) << rendered;
    EXPECT_NE(rendered.find("\"file\": \"src/back\\\\slash.cpp\""), std::string::npos) << rendered;
    EXPECT_NE(rendered.find("\"output\": \"o\\u000aline\\u0001.o\""), std::string::npos) << rendered;
    EXPECT_NE(rendered.find("\"arguments\": []"), std::string::npos) << rendered;
}

/**
 * The build directory is created when missing, and nothing but the database
 * is left in it.
 */
TEST(CompilationDatabaseTest, WritesTheDatabaseIntoANewDirectory)
{
    const TempDirectory temp;
    const std::filesystem::path buildDirectory = temp.path() / "build" / "debug";
    const std::vector<CompileCommand> commands{commandFor("src/main.cpp", "build/debug/obj/hello/src/main.cpp.o")};

    const auto written = writeCompilationDatabase(buildDirectory, commands);

    ASSERT_TRUE(written.has_value()) << written.error().code.message();
    EXPECT_EQ(temp.readFile(buildDirectory / "compile_commands.json"), renderCompilationDatabase(commands));
    EXPECT_EQ(entriesOf(buildDirectory), std::vector<std::string>{"compile_commands.json"});
}

/**
 * A later build replaces the database of an earlier one.
 */
TEST(CompilationDatabaseTest, ReplacesAnEarlierDatabase)
{
    const TempDirectory temp;
    const std::filesystem::path buildDirectory = temp.path() / "build" / "debug";
    ASSERT_TRUE(
        writeCompilationDatabase(buildDirectory, {commandFor("src/main.cpp", "build/debug/obj/hello/src/main.cpp.o")})
            .has_value());

    const auto written = writeCompilationDatabase(buildDirectory, {});

    ASSERT_TRUE(written.has_value()) << written.error().code.message();
    EXPECT_EQ(temp.readFile(buildDirectory / "compile_commands.json"), "[]\n");
    EXPECT_EQ(entriesOf(buildDirectory), std::vector<std::string>{"compile_commands.json"});
}

/**
 * A file where the build directory belongs stops the write at creating the
 * directory, which is the path reported.
 */
TEST(CompilationDatabaseTest, ReportsABuildDirectoryItCannotCreate)
{
    const TempDirectory temp;
    temp.writeFile("build", "not a directory\n");
    const std::filesystem::path buildDirectory = temp.path() / "build" / "debug";

    const auto written = writeCompilationDatabase(buildDirectory, {});

    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error().step, DatabaseWriteStep::CreateDirectory);
    EXPECT_EQ(written.error().path, buildDirectory);
    EXPECT_TRUE(written.error().code);
}

/**
 * A directory where the database belongs stops the write at the file, which
 * is the path reported, and the staged file is removed.
 */
TEST(CompilationDatabaseTest, ReportsADatabaseItCannotWrite)
{
    const TempDirectory temp;
    const std::filesystem::path buildDirectory = temp.path() / "build" / "debug";
    temp.writeFile("build/debug/compile_commands.json/occupied", "a directory in the way\n");

    const auto written = writeCompilationDatabase(buildDirectory, {});

    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error().step, DatabaseWriteStep::WriteFile);
    EXPECT_EQ(written.error().path, buildDirectory / "compile_commands.json");
    EXPECT_TRUE(written.error().code);
    EXPECT_EQ(entriesOf(buildDirectory), std::vector<std::string>{"compile_commands.json"});
}
