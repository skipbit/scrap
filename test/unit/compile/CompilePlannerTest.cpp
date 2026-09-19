#include <gtest/gtest.h>

#include "compile/CompileCommand.h"
#include "compile/CompilePlanner.h"
#include "project/Manifest.h"
#include "project/SourceCollector.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

using namespace scrap::Compile;
using scrap::Project::Package;
using scrap::Project::Target;
using scrap::Project::TargetKind;
using scrap::Project::TargetSources;

namespace {

const std::filesystem::path ProjectRoot = "/home/me/hello";
const std::filesystem::path BuildDirectory = "build/debug";
const std::filesystem::path Compiler = "/usr/bin/c++";

Package packageWithStandard(const char* standard)
{
    return Package{.name = "hello", .version = "0.1.0", .standard = standard};
}

TargetSources
executableWithSources(const char* name, const char* entryPoint, std::vector<std::filesystem::path> sources)
{
    return TargetSources{.target = Target{.kind = TargetKind::Executable, .name = name, .entryPoint = entryPoint},
                         .sources = std::move(sources)};
}

std::vector<std::filesystem::path> outputsOf(const std::vector<CompileCommand>& commands)
{
    std::vector<std::filesystem::path> outputs;
    outputs.reserve(commands.size());
    for (const CompileCommand& command : commands) {
        outputs.push_back(command.output);
    }
    return outputs;
}

}  // namespace

/**
 * A command runs in the project root and names the source and the object file
 * relative to it, so diagnostics and __FILE__ carry the short path.
 */
TEST(CompilePlannerTest, CompilesASourceFromTheProjectRoot)
{
    const auto commands = planCompileCommands(ProjectRoot,
                                              BuildDirectory,
                                              packageWithStandard("23"),
                                              {executableWithSources("hello", "src/main.cpp", {"src/main.cpp"})},
                                              Compiler);

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].directory, ProjectRoot);
    EXPECT_EQ(commands[0].file, "src/main.cpp");
    EXPECT_EQ(commands[0].output, "build/debug/obj/hello/src/main.cpp.o");
    EXPECT_EQ(commands[0].arguments,
              (std::vector<std::string>{"/usr/bin/c++",
                                        "-std=c++23",
                                        "-I",
                                        "include",
                                        "-c",
                                        "src/main.cpp",
                                        "-o",
                                        "build/debug/obj/hello/src/main.cpp.o"}));
}

/**
 * The language standard is the one the manifest states.
 */
TEST(CompilePlannerTest, TakesTheStandardFromTheManifest)
{
    const auto commands = planCompileCommands(ProjectRoot,
                                              BuildDirectory,
                                              packageWithStandard("20"),
                                              {executableWithSources("hello", "src/main.cpp", {"src/main.cpp"})},
                                              Compiler);

    ASSERT_EQ(commands.size(), 1);
    ASSERT_GE(commands[0].arguments.size(), 2);
    EXPECT_EQ(commands[0].arguments[1], "-std=c++20");
}

/**
 * A source two targets share is compiled once for each, into an object file
 * of its own, in the order the targets and their sources were given.
 */
TEST(CompilePlannerTest, CompilesASharedSourceOnceForEachTarget)
{
    const auto commands =
        planCompileCommands(ProjectRoot,
                            BuildDirectory,
                            packageWithStandard("23"),
                            {executableWithSources("app", "src/main.cpp", {"src/main.cpp", "src/shared.cpp"}),
                             executableWithSources("tool", "src/tool.cpp", {"src/shared.cpp", "src/tool.cpp"})},
                            Compiler);

    EXPECT_EQ(outputsOf(commands),
              (std::vector<std::filesystem::path>{"build/debug/obj/app/src/main.cpp.o",
                                                  "build/debug/obj/app/src/shared.cpp.o",
                                                  "build/debug/obj/tool/src/shared.cpp.o",
                                                  "build/debug/obj/tool/src/tool.cpp.o"}));
}

/**
 * The object file mirrors the source's directories and keeps its extension, so
 * sources that share a stem stay apart.
 */
TEST(CompilePlannerTest, KeepsSourcesThatShareAStemApart)
{
    const auto commands = planCompileCommands(
        ProjectRoot,
        BuildDirectory,
        packageWithStandard("23"),
        {executableWithSources("hello", "src/main.cpp", {"src/a/x.cpp", "src/b/x.cpp", "src/main.cpp", "src/x.cc"})},
        Compiler);

    EXPECT_EQ(outputsOf(commands),
              (std::vector<std::filesystem::path>{"build/debug/obj/hello/src/a/x.cpp.o",
                                                  "build/debug/obj/hello/src/b/x.cpp.o",
                                                  "build/debug/obj/hello/src/main.cpp.o",
                                                  "build/debug/obj/hello/src/x.cc.o"}));
}

/**
 * An entry point outside src/ is compiled where it is, and its object file
 * mirrors that place.
 */
TEST(CompilePlannerTest, CompilesAnEntryPointOutsideTheSourceDirectory)
{
    const auto commands = planCompileCommands(ProjectRoot,
                                              BuildDirectory,
                                              packageWithStandard("23"),
                                              {executableWithSources("gen", "tools/gen.cpp", {"tools/gen.cpp"})},
                                              Compiler);

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].file, "tools/gen.cpp");
    EXPECT_EQ(commands[0].output, "build/debug/obj/gen/tools/gen.cpp.o");
}

/**
 * Without a target there is nothing to compile.
 */
TEST(CompilePlannerTest, PlansNothingWithoutATarget)
{
    EXPECT_TRUE(planCompileCommands(ProjectRoot, BuildDirectory, packageWithStandard("23"), {}, Compiler).empty());
}

/**
 * Object files go below the build directory the caller gives, so the database
 * and the objects it names cannot point at different builds.
 */
TEST(CompilePlannerTest, PlacesObjectFilesInTheBuildDirectoryGiven)
{
    const auto commands = planCompileCommands(ProjectRoot,
                                              "build/release",
                                              packageWithStandard("23"),
                                              {executableWithSources("hello", "src/main.cpp", {"src/main.cpp"})},
                                              Compiler);

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].output, "build/release/obj/hello/src/main.cpp.o");
    EXPECT_EQ(commands[0].arguments.back(), "build/release/obj/hello/src/main.cpp.o");
}

/**
 * A declared entry point that starts with '-' reaches the compiler as a file,
 * never as an option.
 */
TEST(CompilePlannerTest, PassesASourceThatLooksLikeAnOptionAsAFile)
{
    const auto commands = planCompileCommands(ProjectRoot,
                                              BuildDirectory,
                                              packageWithStandard("23"),
                                              {executableWithSources("x", "-fplugin=evil.so", {"-fplugin=evil.so"})},
                                              Compiler);

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].file, "./-fplugin=evil.so");
    EXPECT_EQ(commands[0].output, "build/debug/obj/x/-fplugin=evil.so.o");
    EXPECT_EQ(commands[0].arguments,
              (std::vector<std::string>{"/usr/bin/c++",
                                        "-std=c++23",
                                        "-I",
                                        "include",
                                        "-c",
                                        "./-fplugin=evil.so",
                                        "-o",
                                        "build/debug/obj/x/-fplugin=evil.so.o"}));
}
