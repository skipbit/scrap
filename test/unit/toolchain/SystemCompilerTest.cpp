#include <gtest/gtest.h>

#include "toolchain/SystemCompiler.h"

#include "support/TempDirectory.h"

#include <filesystem>
#include <string>
#include <system_error>

using namespace scrap::Toolchain;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * Create a program that can be run below the temp directory.
 */
auto createExecutable(const TempDirectory& temp, const std::string& relative) -> std::filesystem::path
{
    const std::filesystem::path path = temp.writeFile(relative, "#!/bin/sh\nexit 0\n");
    std::error_code ec;
    std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add, ec);
    if (ec) {
        ADD_FAILURE() << "cannot make " << path << " runnable: " << ec.message();
    }
    return path;
}

/**
 * The path as the detection reports it, so a temp directory reached through a
 * symbolic link does not make an equal answer look different.
 */
auto asReported(const std::filesystem::path& path) -> std::filesystem::path
{
    std::error_code ec;
    std::filesystem::path absolute = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return path;
    }
    return absolute;
}

}  // namespace

/**
 * What CXX names is used, and is reported as coming from the variable.
 */
TEST(SystemCompilerTest, UsesTheCompilerTheVariableNames)
{
    const TempDirectory temp;
    createExecutable(temp, "first/c++");
    const auto named = createExecutable(temp, "first/my-compiler");

    const auto detected = detectSystemCompiler("my-compiler", {temp.path() / "first"});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, asReported(named));
    EXPECT_TRUE(detected->path.is_absolute());
    EXPECT_EQ(detected->origin, CompilerOrigin::CompilerVariable);
}

/**
 * A path with a directory in it names one program, so it is read as it stands
 * rather than looked for on the search paths.
 */
TEST(SystemCompilerTest, ReadsAPathTheVariableNamesWithoutSearching)
{
    const TempDirectory temp;
    const auto named = createExecutable(temp, "elsewhere/g++");
    temp.makeDirectory("first");

    const auto detected = detectSystemCompiler(named.string(), {temp.path() / "first"});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, asReported(named));
    EXPECT_EQ(detected->origin, CompilerOrigin::CompilerVariable);
}

/**
 * The default compiler answers before the compilers known by name.
 */
TEST(SystemCompilerTest, PrefersTheDefaultCompilerOverAKnownName)
{
    const TempDirectory temp;
    const auto standard = createExecutable(temp, "first/c++");
    createExecutable(temp, "first/g++");

    const auto detected = detectSystemCompiler("", {temp.path() / "first"});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, asReported(standard));
    EXPECT_EQ(detected->origin, CompilerOrigin::DefaultOnPath);
}

/**
 * Without a default compiler, a compiler known by name is used.
 */
TEST(SystemCompilerTest, UsesACompilerKnownByName)
{
    const TempDirectory temp;
    const auto known = createExecutable(temp, "first/clang++");

    const auto detected = detectSystemCompiler("", {temp.path() / "first"});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, asReported(known));
    EXPECT_EQ(detected->origin, CompilerOrigin::KnownName);
}

/**
 * The known names are looked for in the order they are written.
 */
TEST(SystemCompilerTest, LooksForTheKnownNamesInOrder)
{
    const TempDirectory temp;
    const auto first = createExecutable(temp, "first/g++");
    createExecutable(temp, "first/clang++");

    const auto detected = detectSystemCompiler("", {temp.path() / "first"});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, asReported(first));
}

/**
 * The directories are searched in the order they are given.
 */
TEST(SystemCompilerTest, SearchesTheDirectoriesInOrder)
{
    const TempDirectory temp;
    const auto earlier = createExecutable(temp, "first/c++");
    createExecutable(temp, "second/c++");

    const auto detected = detectSystemCompiler("", {temp.path() / "first", temp.path() / "second"});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, asReported(earlier));
}

/**
 * A file that cannot be run is not a compiler, whatever it is called.
 */
TEST(SystemCompilerTest, IgnoresAFileThatCannotBeRun)
{
    const TempDirectory temp;
    temp.writeFile("first/c++", "not executable");
    const auto runnable = createExecutable(temp, "first/g++");

    const auto detected = detectSystemCompiler("", {temp.path() / "first"});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, asReported(runnable));
    EXPECT_EQ(detected->origin, CompilerOrigin::KnownName);
}

/**
 * A request that cannot be met is reported rather than passed over: a build
 * that quietly used another compiler would answer a question nobody asked.
 */
TEST(SystemCompilerTest, ReportsARequestThatCannotBeRun)
{
    const TempDirectory temp;
    createExecutable(temp, "first/c++");
    const std::string absent = (temp.path() / "elsewhere" / "absent-compiler").string();

    const auto detected = detectSystemCompiler(absent, {temp.path() / "first"});

    ASSERT_FALSE(detected.has_value());
    EXPECT_EQ(detected.error().requested, absent);
}

/**
 * CXX is the path of one program, so a value carrying a launcher or a flag
 * names no file and is reported.
 */
TEST(SystemCompilerTest, ReportsARequestThatCarriesMoreThanAPath)
{
    const TempDirectory temp;
    createExecutable(temp, "first/g++");
    createExecutable(temp, "first/c++");

    const auto detected = detectSystemCompiler("ccache g++", {temp.path() / "first"});

    ASSERT_FALSE(detected.has_value());
    EXPECT_EQ(detected.error().requested, "ccache g++");
}

/**
 * A system with no compiler at all is a different answer from a request that
 * could not be met, and carries no request.
 */
TEST(SystemCompilerTest, ReportsNothingRequestedWhenTheSystemHasNoCompiler)
{
    const TempDirectory temp;
    temp.makeDirectory("first");

    const auto detected = detectSystemCompiler("", {temp.path() / "first"});

    ASSERT_FALSE(detected.has_value());
    EXPECT_TRUE(detected.error().requested.empty());
}

/**
 * Without any directory to search there is nowhere to look.
 */
TEST(SystemCompilerTest, FindsNothingWithoutSearchPaths)
{
    EXPECT_FALSE(detectSystemCompiler("", {}).has_value());
}
