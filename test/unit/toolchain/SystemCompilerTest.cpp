#include <gtest/gtest.h>

#include "toolchain/SystemCompiler.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace scrap::Toolchain;

/**
 * Test fixture providing directories that stand in for the ones PATH lists.
 */
class SystemCompilerTest : public ::testing::Test {
protected:
    /**
     * Create two per-test directories.
     *
     * Each test case runs as its own ctest entry and ctest may run them in
     * parallel, so the directory name must be unique per test case.
     */
    void SetUp() override
    {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        root_ = std::filesystem::temp_directory_path() / (std::string("scrap_compiler_test_") + info->name());
        first_ = root_ / "first";
        second_ = root_ / "second";
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(first_);
        std::filesystem::create_directories(second_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(root_);
    }

    /**
     * Create a program that can be run.
     */
    std::filesystem::path createExecutable(const std::filesystem::path& directory, const std::string& name) const
    {
        const auto path = directory / name;
        std::ofstream(path) << "#!/bin/sh\nexit 0\n";
        std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
        return path;
    }

    /**
     * Create a file of the same name that cannot be run.
     */
    void createNonExecutable(const std::filesystem::path& directory, const std::string& name) const
    {
        std::ofstream(directory / name) << "not executable";
    }

    std::filesystem::path root_;
    std::filesystem::path first_;
    std::filesystem::path second_;
};

/**
 * What CXX names is used, and is reported as coming from the variable.
 */
TEST_F(SystemCompilerTest, UsesTheCompilerTheVariableNames)
{
    createExecutable(first_, "c++");
    const auto named = createExecutable(first_, "my-compiler");

    const auto detected = detectSystemCompiler("my-compiler", {first_});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, named);
    EXPECT_EQ(detected->origin, CompilerOrigin::CompilerVariable);
}

/**
 * A path with a directory in it names one program, so it is read as it stands
 * rather than looked for on the search paths.
 */
TEST_F(SystemCompilerTest, ReadsAPathTheVariableNamesWithoutSearching)
{
    const auto named = createExecutable(second_, "g++");

    const auto detected = detectSystemCompiler(named.string(), {first_});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, named);
    EXPECT_EQ(detected->origin, CompilerOrigin::CompilerVariable);
}

/**
 * The default compiler answers before the compilers known by name.
 */
TEST_F(SystemCompilerTest, PrefersTheDefaultCompilerOverAKnownName)
{
    const auto standard = createExecutable(first_, "c++");
    createExecutable(first_, "g++");

    const auto detected = detectSystemCompiler("", {first_});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, standard);
    EXPECT_EQ(detected->origin, CompilerOrigin::DefaultOnPath);
}

/**
 * Without a default compiler, a compiler known by name is used.
 */
TEST_F(SystemCompilerTest, UsesACompilerKnownByName)
{
    const auto known = createExecutable(first_, "clang++");

    const auto detected = detectSystemCompiler("", {first_});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, known);
    EXPECT_EQ(detected->origin, CompilerOrigin::KnownName);
}

/**
 * The known names are looked for in the order they are written.
 */
TEST_F(SystemCompilerTest, LooksForTheKnownNamesInOrder)
{
    const auto first = createExecutable(first_, "g++");
    createExecutable(first_, "clang++");

    const auto detected = detectSystemCompiler("", {first_});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, first);
}

/**
 * The directories are searched in the order they are given.
 */
TEST_F(SystemCompilerTest, SearchesTheDirectoriesInOrder)
{
    const auto earlier = createExecutable(first_, "c++");
    createExecutable(second_, "c++");

    const auto detected = detectSystemCompiler("", {first_, second_});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, earlier);
}

/**
 * A file that cannot be run is not a compiler, whatever it is called.
 */
TEST_F(SystemCompilerTest, IgnoresAFileThatCannotBeRun)
{
    createNonExecutable(first_, "c++");
    const auto runnable = createExecutable(first_, "g++");

    const auto detected = detectSystemCompiler("", {first_});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, runnable);
    EXPECT_EQ(detected->origin, CompilerOrigin::KnownName);
}

/**
 * A name CXX gives that leads nowhere moves the search on, which is what
 * searching in order and using the first one found amounts to.
 */
TEST_F(SystemCompilerTest, MovesOnWhenTheVariableNamesSomethingAbsent)
{
    const auto standard = createExecutable(first_, "c++");

    const auto detected = detectSystemCompiler((second_ / "absent-compiler").string(), {first_});

    ASSERT_TRUE(detected.has_value());
    EXPECT_EQ(detected->path, standard);
    EXPECT_EQ(detected->origin, CompilerOrigin::DefaultOnPath);
}

/**
 * A system with no compiler at all yields nothing to report.
 */
TEST_F(SystemCompilerTest, FindsNothingWhenNoCompilerIsThere)
{
    EXPECT_FALSE(detectSystemCompiler("", {first_, second_}).has_value());
}

/**
 * Without any directory to search there is nowhere to look.
 */
TEST_F(SystemCompilerTest, FindsNothingWithoutSearchPaths)
{
    EXPECT_FALSE(detectSystemCompiler("g++", {}).has_value());
}
