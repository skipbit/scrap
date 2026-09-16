#include <gtest/gtest.h>

#include "project/driver/DiskProjectFileSystem.h"

#include "TempDirectory.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

using scrap::Project::DiskProjectFileSystem;
using scrap::TestSupport::TempDirectory;

namespace {

auto readFile(const std::filesystem::path& file) -> std::string
{
    std::ifstream input(file, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

}  // namespace

/**
 * A new file is created with the content it was given.
 */
TEST(DiskProjectFileSystemTest, WritesANewFile)
{
    const TempDirectory temp;
    DiskProjectFileSystem files;

    const std::error_code code = files.writeNewFile(temp.path() / "note.txt", "hello\n");

    EXPECT_FALSE(static_cast<bool>(code)) << code.message();
    EXPECT_EQ(readFile(temp.path() / "note.txt"), "hello\n");
}

/**
 * An existing file keeps its content, and the call reports that it is there.
 */
TEST(DiskProjectFileSystemTest, KeepsAnExistingFile)
{
    const TempDirectory temp;
    const auto file = temp.writeFile("note.txt", "keep me\n");
    DiskProjectFileSystem files;

    const std::error_code code = files.writeNewFile(file, "overwritten\n");

    EXPECT_EQ(code, std::errc::file_exists);
    EXPECT_EQ(readFile(file), "keep me\n");
}

/**
 * A symbolic link at the path keeps its target intact: the write stops at the
 * link itself.
 */
TEST(DiskProjectFileSystemTest, WritesThroughNoSymbolicLink)
{
    const TempDirectory temp;
    const auto target = temp.writeFile("target.txt", "keep me\n");
    std::filesystem::create_symlink(target, temp.path() / "link.txt");
    DiskProjectFileSystem files;

    const std::error_code code = files.writeNewFile(temp.path() / "link.txt", "overwritten\n");

    EXPECT_TRUE(static_cast<bool>(code));
    EXPECT_EQ(readFile(target), "keep me\n");
}

/**
 * A directory that exists is reported as such, so the caller can tell it from
 * a directory it created itself.
 */
TEST(DiskProjectFileSystemTest, ReportsAnExistingDirectory)
{
    const TempDirectory temp;
    temp.makeDirectory("here");
    DiskProjectFileSystem files;

    EXPECT_EQ(files.createDirectory(temp.path() / "here"), std::errc::file_exists);
    EXPECT_FALSE(static_cast<bool>(files.createDirectory(temp.path() / "fresh")));
    EXPECT_TRUE(std::filesystem::is_directory(temp.path() / "fresh"));
}

/**
 * Removing a directory takes everything below it.
 */
TEST(DiskProjectFileSystemTest, RemovesADirectoryTree)
{
    const TempDirectory temp;
    temp.writeFile("tree/below/note.txt", "content\n");
    DiskProjectFileSystem files;

    EXPECT_FALSE(static_cast<bool>(files.removeAll(temp.path() / "tree")));
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "tree"));
}
