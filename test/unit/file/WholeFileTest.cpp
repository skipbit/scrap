#include "file/WholeFile.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <system_error>

using namespace scrap::File;
using scrap::TestSupport::TempDirectory;

/**
 * The bytes of a file arrive as they are, and an empty file is not a
 * failure.
 */
TEST(WholeFileTest, ReadsTheBytesOfAFile)
{
    const TempDirectory temp;
    temp.writeFile("text", std::string{ "a\nb\0c", 5 });
    temp.writeFile("empty", "");

    EXPECT_EQ(readWholeFile(temp.path() / "text"), (std::string{ "a\nb\0c", 5 }));
    EXPECT_EQ(readWholeFile(temp.path() / "empty"), "");
}

/**
 * Nothing at the path, and a directory, cannot be opened as a file.
 */
TEST(WholeFileTest, CannotOpenWhatIsNotAFile)
{
    const TempDirectory temp;
    std::filesystem::create_directory(temp.path() / "directory");

    EXPECT_EQ(readWholeFile(temp.path() / "missing"), std::unexpected(ReadFailure::CannotOpen));
    EXPECT_EQ(readWholeFile(temp.path() / "directory"), std::unexpected(ReadFailure::CannotOpen));
}

/**
 * A file is replaced whole, and nothing staged is left beside it.
 */
TEST(WholeFileTest, ReplacesAFileWhole)
{
    const TempDirectory temp;
    temp.writeFile("out", "the previous content, longer than the new one");

    EXPECT_FALSE(replaceFile(temp.path() / "out", "new"));

    EXPECT_EQ(temp.readFile("out"), "new");
    std::size_t entries = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator{ temp.path() }) {
        ++entries;
    }
    EXPECT_EQ(entries, 1);
}

/**
 * A directory that does not exist is reported, and nothing is created.
 */
TEST(WholeFileTest, ReportsADirectoryThatDoesNotExist)
{
    const TempDirectory temp;

    const std::error_code failure = replaceFile(temp.path() / "missing" / "out", "new");

    EXPECT_EQ(failure, std::errc::no_such_file_or_directory);
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "missing"));
}
