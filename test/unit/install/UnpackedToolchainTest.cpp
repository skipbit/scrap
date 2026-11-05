#include "install/UnpackedToolchain.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace scrap::Install;
using scrap::TestSupport::TempDirectory;

/**
 * An archive that holds everything in one directory of its own starts there.
 */
TEST(UnpackedToolchainTest, StripsTheOneDirectoryTheContentCameIn)
{
    const TempDirectory unpacked;
    unpacked.writeFile("gcc-15.2.0/bin/g++", "");

    EXPECT_EQ(contentRoot(unpacked.path()), unpacked.path() / "gcc-15.2.0");
}

TEST(UnpackedToolchainTest, KeepsTheDirectoryThatHoldsTwoEntries)
{
    const TempDirectory unpacked;
    unpacked.writeFile("gcc-15.2.0/bin/g++", "");
    unpacked.writeFile("README", "");

    EXPECT_EQ(contentRoot(unpacked.path()), unpacked.path());
}

/**
 * A lone bin is the toolchain's own directory, not a directory around it.
 */
TEST(UnpackedToolchainTest, KeepsTheDirectoryThatHoldsOnlyBin)
{
    const TempDirectory unpacked;
    unpacked.writeFile("bin/g++", "");

    EXPECT_EQ(contentRoot(unpacked.path()), unpacked.path());
}

TEST(UnpackedToolchainTest, KeepsTheDirectoryThatHoldsOnlyALinkToADirectory)
{
    const TempDirectory unpacked;
    const TempDirectory elsewhere;
    std::filesystem::create_directory_symlink(elsewhere.path(), unpacked.path() / "link");

    EXPECT_EQ(contentRoot(unpacked.path()), unpacked.path());
}

TEST(UnpackedToolchainTest, KeepsTheDirectoryThatHoldsOnlyAFile)
{
    const TempDirectory unpacked;
    unpacked.writeFile("README", "");

    EXPECT_EQ(contentRoot(unpacked.path()), unpacked.path());
}

TEST(UnpackedToolchainTest, KeepsAnEmptyDirectory)
{
    const TempDirectory unpacked;

    EXPECT_EQ(contentRoot(unpacked.path()), unpacked.path());
}

TEST(UnpackedToolchainTest, FindsRuntimeLibrariesInLibAndLib64)
{
    const TempDirectory root;
    root.writeFile("lib64/libstdc++.so.6", "");
    root.writeFile("lib/libstdc++.so.6", "");

    const std::vector<std::filesystem::path> expected{ "lib", "lib64" };
    EXPECT_EQ(findRuntimeLibraryDirectories(root.path()), expected);
}

/**
 * The record must not depend on the order the file system lists directories in.
 */
TEST(UnpackedToolchainTest, ListsTheDirectoriesInOrder)
{
    const TempDirectory root;
    std::vector<std::filesystem::path> expected;
    for (const char* const directory : { "lib/a", "lib/b", "lib/c", "lib/d", "lib/e", "lib/f", "lib/g", "lib/h" }) {
        root.writeFile(std::string{ directory } + "/libstdc++.so.6", "");
        expected.emplace_back(directory);
    }

    EXPECT_EQ(findRuntimeLibraryDirectories(root.path()), expected);
}

TEST(UnpackedToolchainTest, NamesADirectoryOnceHoweverManyLibrariesItHolds)
{
    const TempDirectory root;
    root.writeFile("lib/libstdc++.so.6", "");
    root.writeFile("lib/libstdc++.so", "");
    root.writeFile("lib/libc++.so.1", "");

    const std::vector<std::filesystem::path> expected{ "lib" };
    EXPECT_EQ(findRuntimeLibraryDirectories(root.path()), expected);
}

TEST(UnpackedToolchainTest, PassesOverALibraryThatIsNoRuntimeLibrary)
{
    const TempDirectory root;
    root.writeFile("lib/libm.so.6", "");

    EXPECT_TRUE(findRuntimeLibraryDirectories(root.path()).empty());
}

TEST(UnpackedToolchainTest, FindsTheRuntimeLibraryOfMacOs)
{
    const TempDirectory root;
    root.writeFile("lib/libc++.1.dylib", "");

    const std::vector<std::filesystem::path> expected{ "lib" };
    EXPECT_EQ(findRuntimeLibraryDirectories(root.path()), expected);
}

TEST(UnpackedToolchainTest, FindsNothingWhereThereIsNoLibDirectory)
{
    const TempDirectory root;
    root.writeFile("bin/g++", "");

    EXPECT_TRUE(findRuntimeLibraryDirectories(root.path()).empty());
}
