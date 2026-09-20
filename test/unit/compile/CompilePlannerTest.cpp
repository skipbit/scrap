#include <gtest/gtest.h>

#include "compile/CompileCommand.h"
#include "compile/CompilePlanner.h"
#include "compile/CompilerDriver.h"
#include "compile/LinkCommand.h"
#include "project/LanguageStandard.h"
#include "project/Manifest.h"
#include "project/SourceCollector.h"
#include "toolchain/CompilerIdentity.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

using namespace scrap::Compile;
using scrap::Project::LanguageStandard;
using scrap::Project::Target;
using scrap::Project::TargetKind;
using scrap::Project::TargetSources;
using scrap::Toolchain::CompilerFamily;
using scrap::Toolchain::CompilerIdentity;

namespace {

const std::filesystem::path ProjectRoot = "/home/me/hello";
const std::filesystem::path Compiler = "/usr/bin/c++";
const CompilerIdentity Gcc13{.family = CompilerFamily::Gcc, .version = {.major = 13, .minor = 3, .patch = 0}};

BuildSettings settingsFor(const CompilerIdentity& identity,
                          LanguageStandard standard = LanguageStandard::Cxx23,
                          const std::filesystem::path& buildDirectory = "build/debug")
{
    return BuildSettings{.projectRoot = ProjectRoot,
                         .buildDirectory = buildDirectory,
                         .compiler = Compiler,
                         .driver = CompilerDriver{identity},
                         .standard = standard};
}

TargetSources
targetWithSources(TargetKind kind, const char* name, const char* entryPoint, std::vector<std::filesystem::path> sources)
{
    return TargetSources{.target = Target{.kind = kind, .name = name, .entryPoint = entryPoint},
                         .sources = std::move(sources)};
}

TargetSources
executableWithSources(const char* name, const char* entryPoint, std::vector<std::filesystem::path> sources)
{
    return targetWithSources(TargetKind::Executable, name, entryPoint, std::move(sources));
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
 * relative to it, so diagnostics and __FILE__ carry the short path. It builds
 * for debugging, with the common warnings on and colour kept in diagnostics.
 */
TEST(CompilePlannerTest, CompilesASourceFromTheProjectRoot)
{
    const auto commands =
        planCompileCommands(settingsFor(Gcc13), {executableWithSources("hello", "src/main.cpp", {"src/main.cpp"})});

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].target, "hello");
    EXPECT_EQ(commands[0].directory, ProjectRoot);
    EXPECT_EQ(commands[0].file, "src/main.cpp");
    EXPECT_EQ(commands[0].output, "build/debug/obj/hello/src/main.cpp.o");
    EXPECT_EQ(commands[0].arguments,
              (std::vector<std::string>{"/usr/bin/c++",
                                        "-std=c++23",
                                        "-g",
                                        "-O0",
                                        "-Wall",
                                        "-Wextra",
                                        "-Wpedantic",
                                        "-fdiagnostics-color=always",
                                        "-I",
                                        "include",
                                        "-c",
                                        "src/main.cpp",
                                        "-o",
                                        "build/debug/obj/hello/src/main.cpp.o"}));
}

/**
 * The standard is the one the manifest states, spelled as the driver spells
 * it for the compiler.
 */
TEST(CompilePlannerTest, SpellsTheStandardAsTheDriverDoes)
{
    const CompilerIdentity clang16{.family = CompilerFamily::Clang, .version = {.major = 16, .minor = 0, .patch = 6}};

    const auto commands = planCompileCommands(settingsFor(clang16, LanguageStandard::Cxx23),
                                              {executableWithSources("hello", "src/main.cpp", {"src/main.cpp"})});

    ASSERT_EQ(commands.size(), 1);
    ASSERT_GE(commands[0].arguments.size(), 2);
    EXPECT_EQ(commands[0].arguments[1], "-std=c++2b");
}

/**
 * A standard the compiler cannot build is still named, by its own name, so
 * the compilation database states the standard the manifest does.
 */
TEST(CompilePlannerTest, NamesAStandardTheCompilerCannotBuild)
{
    const auto commands = planCompileCommands(settingsFor(Gcc13, LanguageStandard::Cxx26),
                                              {executableWithSources("hello", "src/main.cpp", {"src/main.cpp"})});

    ASSERT_EQ(commands.size(), 1);
    ASSERT_GE(commands[0].arguments.size(), 2);
    EXPECT_EQ(commands[0].arguments[1], "-std=c++26");
}

/**
 * A compiler of unknown family is given nothing it might reject beyond what
 * every build asks for.
 */
TEST(CompilePlannerTest, GivesAnUnknownCompilerNoColorOption)
{
    const auto commands = planCompileCommands(settingsFor(CompilerIdentity{}),
                                              {executableWithSources("hello", "src/main.cpp", {"src/main.cpp"})});

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].arguments,
              (std::vector<std::string>{"/usr/bin/c++",
                                        "-std=c++23",
                                        "-g",
                                        "-O0",
                                        "-Wall",
                                        "-Wextra",
                                        "-Wpedantic",
                                        "-I",
                                        "include",
                                        "-c",
                                        "src/main.cpp",
                                        "-o",
                                        "build/debug/obj/hello/src/main.cpp.o"}));
}

/**
 * A source two targets share is compiled once for each, into an object file
 * of its own, in the order the targets and their sources were given.
 */
TEST(CompilePlannerTest, CompilesASharedSourceOnceForEachTarget)
{
    const auto commands =
        planCompileCommands(settingsFor(Gcc13),
                            {executableWithSources("app", "src/main.cpp", {"src/main.cpp", "src/shared.cpp"}),
                             executableWithSources("tool", "src/tool.cpp", {"src/shared.cpp", "src/tool.cpp"})});

    EXPECT_EQ(outputsOf(commands),
              (std::vector<std::filesystem::path>{"build/debug/obj/app/src/main.cpp.o",
                                                  "build/debug/obj/app/src/shared.cpp.o",
                                                  "build/debug/obj/tool/src/shared.cpp.o",
                                                  "build/debug/obj/tool/src/tool.cpp.o"}));
    ASSERT_EQ(commands.size(), 4);
    EXPECT_EQ(commands[1].target, "app");
    EXPECT_EQ(commands[2].target, "tool");
}

/**
 * The object file mirrors the source's directories and keeps its extension, so
 * sources that share a stem stay apart.
 */
TEST(CompilePlannerTest, KeepsSourcesThatShareAStemApart)
{
    const auto commands = planCompileCommands(
        settingsFor(Gcc13),
        {executableWithSources("hello", "src/main.cpp", {"src/a/x.cpp", "src/b/x.cpp", "src/main.cpp", "src/x.cc"})});

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
    const auto commands =
        planCompileCommands(settingsFor(Gcc13), {executableWithSources("gen", "tools/gen.cpp", {"tools/gen.cpp"})});

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].file, "tools/gen.cpp");
    EXPECT_EQ(commands[0].output, "build/debug/obj/gen/tools/gen.cpp.o");
}

/**
 * Without a target there is nothing to compile or link.
 */
TEST(CompilePlannerTest, PlansNothingWithoutATarget)
{
    EXPECT_TRUE(planCompileCommands(settingsFor(Gcc13), {}).empty());
    EXPECT_TRUE(planLinkCommands(settingsFor(Gcc13), {}).empty());
}

/**
 * Object files and executables go below the build directory the caller
 * gives, so the database and the files it names cannot point at different
 * builds.
 */
TEST(CompilePlannerTest, WritesBelowTheBuildDirectoryGiven)
{
    const auto settings = settingsFor(Gcc13, LanguageStandard::Cxx23, "build/release");
    const std::vector<TargetSources> targets{executableWithSources("hello", "src/main.cpp", {"src/main.cpp"})};

    const auto compiles = planCompileCommands(settings, targets);
    const auto links = planLinkCommands(settings, targets);

    ASSERT_EQ(compiles.size(), 1);
    EXPECT_EQ(compiles[0].output, "build/release/obj/hello/src/main.cpp.o");
    EXPECT_EQ(compiles[0].arguments.back(), "build/release/obj/hello/src/main.cpp.o");
    ASSERT_EQ(links.size(), 1);
    EXPECT_EQ(links[0].output, "build/release/bin/hello");
}

/**
 * A declared entry point that starts with '-' reaches the compiler as a file,
 * never as an option.
 */
TEST(CompilePlannerTest, PassesASourceThatLooksLikeAnOptionAsAFile)
{
    const auto commands =
        planCompileCommands(settingsFor(Gcc13), {executableWithSources("x", "-fplugin=evil.so", {"-fplugin=evil.so"})});

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].file, "./-fplugin=evil.so");
    EXPECT_EQ(commands[0].output, "build/debug/obj/x/-fplugin=evil.so.o");
    ASSERT_GE(commands[0].arguments.size(), 4);
    EXPECT_EQ(commands[0].arguments[commands[0].arguments.size() - 4], "-c");
    EXPECT_EQ(commands[0].arguments[commands[0].arguments.size() - 3], "./-fplugin=evil.so");
}

/**
 * A declared entry point that starts with '@' reaches the compiler as a
 * file, never as the file of options that an argument starting with '@'
 * otherwise names.
 */
TEST(CompilePlannerTest, PassesASourceThatLooksLikeAFileOfOptionsAsAFile)
{
    const auto commands =
        planCompileCommands(settingsFor(Gcc13), {executableWithSources("x", "@options.cpp", {"@options.cpp"})});

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].file, "./@options.cpp");
    EXPECT_EQ(commands[0].output, "build/debug/obj/x/@options.cpp.o");
    ASSERT_GE(commands[0].arguments.size(), 4);
    EXPECT_EQ(commands[0].arguments[commands[0].arguments.size() - 3], "./@options.cpp");
}

/**
 * An executable is linked in the project root from the object files its
 * sources compile to, in the same order, into bin/ below the build directory.
 */
TEST(CompilePlannerTest, LinksTheObjectFilesOfAnExecutable)
{
    const auto commands = planLinkCommands(
        settingsFor(Gcc13), {executableWithSources("hello", "src/main.cpp", {"src/main.cpp", "src/util.cpp"})});

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].target, "hello");
    EXPECT_EQ(commands[0].directory, ProjectRoot);
    EXPECT_EQ(commands[0].output, "build/debug/bin/hello");
    EXPECT_EQ(commands[0].arguments,
              (std::vector<std::string>{"/usr/bin/c++",
                                        "-fdiagnostics-color=always",
                                        "build/debug/obj/hello/src/main.cpp.o",
                                        "build/debug/obj/hello/src/util.cpp.o",
                                        "-o",
                                        "build/debug/bin/hello"}));
}

/**
 * Each executable links the object files compiled for it, including those of
 * a source it shares with another.
 */
TEST(CompilePlannerTest, LinksEachExecutableFromItsOwnObjectFiles)
{
    const auto commands =
        planLinkCommands(settingsFor(Gcc13),
                         {executableWithSources("app", "src/main.cpp", {"src/main.cpp", "src/shared.cpp"}),
                          executableWithSources("tool", "src/tool.cpp", {"src/shared.cpp", "src/tool.cpp"})});

    ASSERT_EQ(commands.size(), 2);
    EXPECT_EQ(commands[0].output, "build/debug/bin/app");
    EXPECT_EQ(commands[0].arguments[2], "build/debug/obj/app/src/main.cpp.o");
    EXPECT_EQ(commands[0].arguments[3], "build/debug/obj/app/src/shared.cpp.o");
    EXPECT_EQ(commands[1].output, "build/debug/bin/tool");
    EXPECT_EQ(commands[1].arguments[2], "build/debug/obj/tool/src/shared.cpp.o");
    EXPECT_EQ(commands[1].arguments[3], "build/debug/obj/tool/src/tool.cpp.o");
}

/**
 * A library is not linked into anything of its own.
 */
TEST(CompilePlannerTest, LinksNoLibrary)
{
    const auto commands =
        planLinkCommands(settingsFor(Gcc13),
                         {executableWithSources("app", "src/main.cpp", {"src/main.cpp"}),
                          targetWithSources(TargetKind::Library, "core", "src/core.cpp", {"src/core.cpp"})});

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].target, "app");
}

/**
 * A compiler of unknown family links without the colour option.
 */
TEST(CompilePlannerTest, LinksWithoutColorForAnUnknownCompiler)
{
    const auto commands = planLinkCommands(settingsFor(CompilerIdentity{}),
                                           {executableWithSources("hello", "src/main.cpp", {"src/main.cpp"})});

    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands[0].arguments,
              (std::vector<std::string>{
                  "/usr/bin/c++", "build/debug/obj/hello/src/main.cpp.o", "-o", "build/debug/bin/hello"}));
}
