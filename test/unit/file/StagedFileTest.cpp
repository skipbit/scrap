#include "file/StagedFile.h"

#include "file/WholeFile.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace scrap::File;
using scrap::TestSupport::TempDirectory;

TEST(StagedFileTest, PutsWhatWasAppendedAtTheDestinationOnCommit)
{
    const TempDirectory temp;
    const auto destination = temp.path() / "file";

    auto staged = StagedFile::begin(destination);
    ASSERT_TRUE(staged.has_value());
    EXPECT_FALSE(staged->append("one ").operator bool());
    EXPECT_FALSE(staged->append("two").operator bool());
    EXPECT_FALSE(std::filesystem::exists(destination));

    EXPECT_FALSE(staged->commit().operator bool());
    EXPECT_EQ(readWholeFile(destination), "one two");
    EXPECT_EQ(temp.entries(), (std::vector<std::string>{ "file" }));
}

TEST(StagedFileTest, ReplacesAFileAlreadyAtTheDestination)
{
    const TempDirectory temp;
    const auto destination = temp.path() / "file";
    ASSERT_FALSE(writeNewFile(destination, "old").operator bool());

    auto staged = StagedFile::begin(destination);
    ASSERT_TRUE(staged.has_value());
    ASSERT_FALSE(staged->append("new").operator bool());
    EXPECT_EQ(readWholeFile(destination), "old");

    EXPECT_FALSE(staged->commit().operator bool());
    EXPECT_EQ(readWholeFile(destination), "new");
}

TEST(StagedFileTest, LeavesNothingBehindUnlessCommitted)
{
    const TempDirectory temp;
    const auto destination = temp.path() / "file";
    ASSERT_FALSE(writeNewFile(destination, "old").operator bool());

    {
        auto staged = StagedFile::begin(destination);
        ASSERT_TRUE(staged.has_value());
        ASSERT_FALSE(staged->append("new").operator bool());
        EXPECT_EQ(temp.entries().size(), 2U);
    }

    EXPECT_EQ(readWholeFile(destination), "old");
    EXPECT_EQ(temp.entries(), (std::vector<std::string>{ "file" }));
}

TEST(StagedFileTest, TakesTheNextNameWhenOneIsHeld)
{
    const TempDirectory temp;
    const auto destination = temp.path() / "file";

    auto first = StagedFile::begin(destination);
    auto second = StagedFile::begin(destination);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(temp.entries().size(), 2U);

    ASSERT_FALSE(first->append("first").operator bool());
    ASSERT_FALSE(second->append("second").operator bool());
    EXPECT_FALSE(first->commit().operator bool());
    EXPECT_FALSE(second->commit().operator bool());
    EXPECT_EQ(readWholeFile(destination), "second");
}

TEST(StagedFileTest, MovingHandsOverTheFile)
{
    const TempDirectory temp;
    const auto destination = temp.path() / "file";

    auto staged = StagedFile::begin(destination);
    ASSERT_TRUE(staged.has_value());
    std::optional<StagedFile> moved{ std::move(*staged) };
    staged = StagedFile::begin(temp.path() / "other");
    ASSERT_TRUE(staged.has_value());

    ASSERT_FALSE(moved->append("kept").operator bool());
    EXPECT_FALSE(moved->commit().operator bool());
    EXPECT_EQ(readWholeFile(destination), "kept");
}

TEST(StagedFileTest, ReportsADirectoryThatCannotHoldIt)
{
    const TempDirectory temp;

    const auto staged = StagedFile::begin(temp.path() / "missing" / "file");

    ASSERT_FALSE(staged.has_value());
    EXPECT_EQ(staged.error(), std::errc::no_such_file_or_directory);
}

TEST(StagedFileTest, ReportsADestinationItCannotReplace)
{
    const TempDirectory temp;
    const auto destination = temp.path() / "directory";
    std::filesystem::create_directory(destination);
    ASSERT_FALSE(writeNewFile(destination / "held", "").operator bool());

    auto staged = StagedFile::begin(destination);
    ASSERT_TRUE(staged.has_value());

    EXPECT_TRUE(staged->commit().operator bool());
    EXPECT_TRUE(std::filesystem::is_directory(destination));
    EXPECT_EQ(temp.entries(), (std::vector<std::string>{ "directory" }));
}

TEST(StagedFileTest, ACommitAfterTheFirstLeavesTheNextStagedFileAlone)
{
    const TempDirectory temp;
    const auto destination = temp.path() / "file";
    auto first = StagedFile::begin(destination);
    ASSERT_TRUE(first.has_value());
    ASSERT_FALSE(first->commit().operator bool());
    auto second = StagedFile::begin(destination);
    ASSERT_TRUE(second.has_value());
    ASSERT_FALSE(second->append("second").operator bool());

    EXPECT_EQ(first->commit(), std::errc::bad_file_descriptor);

    EXPECT_FALSE(second->commit().operator bool());
    EXPECT_EQ(readWholeFile(destination), "second");
}
