#include "process/ExecutableFile.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>

using scrap::Process::isExecutableFile;
using scrap::TestSupport::TempDirectory;

/**
 * A file this process may run is executable.
 */
TEST(ExecutableFileTest, AcceptsAFileThisProcessMayRun)
{
    const TempDirectory temp;
    temp.writeFile("tool", "#!/bin/sh\n");
    std::filesystem::permissions(temp.path() / "tool", std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);

    EXPECT_TRUE(isExecutableFile(temp.path() / "tool"));
}

/**
 * A file without the permission to run it is not executable.
 */
TEST(ExecutableFileTest, RejectsAFileWithoutPermissionToRun)
{
    const TempDirectory temp;
    temp.writeFile("notes", "not a program\n");

    EXPECT_FALSE(isExecutableFile(temp.path() / "notes"));
}

/**
 * A directory may carry the execute bits, and is still not a program.
 */
TEST(ExecutableFileTest, RejectsADirectory)
{
    const TempDirectory temp;
    temp.makeDirectory("tool");

    EXPECT_FALSE(isExecutableFile(temp.path() / "tool"));
}

/**
 * A path that names nothing is not executable.
 */
TEST(ExecutableFileTest, RejectsAMissingFile)
{
    const TempDirectory temp;

    EXPECT_FALSE(isExecutableFile(temp.path() / "missing"));
}
