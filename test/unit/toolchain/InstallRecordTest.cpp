#include "toolchain/InstallRecord.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

using namespace scrap::Toolchain;

/**
 * The compiler is read as a path relative to the toolchain, and keys this
 * version does not know are passed over.
 */
TEST(InstallRecordTest, ReadsTheCompiler)
{
    const auto record = parseInstallRecord("compiler = \"LLVM-23.1.4-Linux-X64/bin/clang++\"\nlater = 1\n");

    ASSERT_TRUE(record.has_value()) << record.error();
    EXPECT_EQ(record->compiler, std::filesystem::path("LLVM-23.1.4-Linux-X64/bin/clang++"));
}

/**
 * A record without a compiler, or with one that is not a string, cannot be
 * used.
 */
TEST(InstallRecordTest, RefusesARecordWithoutACompiler)
{
    EXPECT_EQ(parseInstallRecord("").error(), "compiler: is missing");
    EXPECT_EQ(parseInstallRecord("compiler = 1\n").error(), "line 1: compiler: must be a string");
    EXPECT_EQ(parseInstallRecord("compiler = \"bin/g++\n").error().substr(0, 8), "line 1: ");
}

/**
 * A compiler outside the toolchain is refused, so a record cannot send a
 * build to another program.
 */
TEST(InstallRecordTest, RefusesACompilerOutsideTheToolchain)
{
    const std::string reason = "line 1: compiler: must be a relative path inside the toolchain";
    EXPECT_EQ(parseInstallRecord("compiler = \"\"\n").error(), reason);
    EXPECT_EQ(parseInstallRecord("compiler = \"/usr/bin/g++\"\n").error(), reason);
    EXPECT_EQ(parseInstallRecord("compiler = \"../gcc/bin/g++\"\n").error(), reason);
    EXPECT_EQ(parseInstallRecord("compiler = \"bin/../../g++\"\n").error(), reason);
}
