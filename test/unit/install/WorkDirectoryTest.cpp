#include "install/WorkDirectory.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>

using namespace scrap::Install;
using scrap::TestSupport::TempDirectory;

TEST(DiscardAbandonedWorkTest, RemovesOnlyWhatIsADayOld)
{
    const TempDirectory work;
    const auto now = std::filesystem::file_time_type::clock::now();
    const std::filesystem::path old = work.makeDirectory("old");
    work.writeFile("old/nested/file", "x");
    const std::filesystem::path young = work.makeDirectory("young");
    std::filesystem::last_write_time(old, now - std::chrono::hours{ 24 });
    std::filesystem::last_write_time(young, now - std::chrono::hours{ 23 });

    discardAbandonedWork(work.path(), now);

    EXPECT_FALSE(std::filesystem::exists(old));
    EXPECT_TRUE(std::filesystem::exists(young));
}

TEST(DiscardAbandonedWorkTest, PassesOverADirectoryThatIsNotThere)
{
    const TempDirectory work;

    discardAbandonedWork(work.path() / "missing", std::filesystem::file_time_type::clock::now());

    EXPECT_FALSE(std::filesystem::exists(work.path() / "missing"));
}
