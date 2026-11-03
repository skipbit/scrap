#include "digest/Sha256File.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <string>
#include <system_error>

using scrap::Digest::sha256OfFile;
using scrap::TestSupport::TempDirectory;

TEST(Sha256FileTest, DigestsAFile)
{
    const TempDirectory directory;

    const auto digest = sha256OfFile(directory.writeFile("file", "abc"));

    ASSERT_TRUE(digest.has_value());
    EXPECT_EQ(*digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256FileTest, DigestsAFileLargerThanOneRead)
{
    const TempDirectory directory;

    const auto digest = sha256OfFile(directory.writeFile("file", std::string(3'000'000, 'a')));

    ASSERT_TRUE(digest.has_value());
    // From sha256sum.
    EXPECT_EQ(*digest, "2a152c894398719c0570f83fac34ac03a0f6e8e474b995c2403aa5434f7b9dd4");
}

TEST(Sha256FileTest, DigestsAnEmptyFile)
{
    const TempDirectory directory;

    const auto digest = sha256OfFile(directory.writeFile("file", ""));

    ASSERT_TRUE(digest.has_value());
    EXPECT_EQ(*digest, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256FileTest, ReportsAMissingFile)
{
    const TempDirectory directory;

    const auto digest = sha256OfFile(directory.path() / "missing");

    ASSERT_FALSE(digest.has_value());
    EXPECT_EQ(digest.error(), std::make_error_code(std::errc::no_such_file_or_directory));
}

TEST(Sha256FileTest, ReportsADirectory)
{
    const TempDirectory directory;

    const auto digest = sha256OfFile(directory.path());

    ASSERT_FALSE(digest.has_value());
    EXPECT_EQ(digest.error(), std::make_error_code(std::errc::is_a_directory));
}
