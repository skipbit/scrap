#include "toolchain/InstallRecord.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

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

/**
 * The runtime library directories are read in order, as paths relative to
 * the toolchain; a record without them names none.
 */
TEST(InstallRecordTest, ReadsTheRuntimeLibraryDirectories)
{
    const auto record = parseInstallRecord("compiler = \"bin/g++\"\nruntime-library-dirs = [\"lib64\", \"lib/gcc\"]\n");

    ASSERT_TRUE(record.has_value()) << record.error();
    const std::vector<std::filesystem::path> expected{ "lib64", "lib/gcc" };
    EXPECT_EQ(record->runtimeLibraryDirectories, expected);

    const auto without = parseInstallRecord("compiler = \"bin/g++\"\n");
    ASSERT_TRUE(without.has_value()) << without.error();
    EXPECT_TRUE(without->runtimeLibraryDirectories.empty());
}

/**
 * Runtime library directories that are not a list of strings, or that lead
 * outside the toolchain, are refused, so a record cannot point the
 * executables built with it at libraries elsewhere.
 */
TEST(InstallRecordTest, RefusesRuntimeLibraryDirectoriesOutsideTheToolchain)
{
    EXPECT_EQ(parseInstallRecord("compiler = \"bin/g++\"\nruntime-library-dirs = \"lib64\"\n").error(),
              "line 2: runtime-library-dirs: must be an array of strings");
    EXPECT_EQ(parseInstallRecord("compiler = \"bin/g++\"\nruntime-library-dirs = [1]\n").error(),
              "line 2: runtime-library-dirs: must be a string");
    const std::string reason = "line 2: runtime-library-dirs: must be a relative path inside the toolchain";
    EXPECT_EQ(parseInstallRecord("compiler = \"bin/g++\"\nruntime-library-dirs = [\"lib64\", \"/usr/lib\"]\n").error(), reason);
    EXPECT_EQ(parseInstallRecord("compiler = \"bin/g++\"\nruntime-library-dirs = [\"../lib\"]\n").error(), reason);
    EXPECT_EQ(parseInstallRecord("compiler = \"bin/g++\"\nruntime-library-dirs = [\"\"]\n").error(), reason);
}

TEST(InstallRecordTest, ReadsTheIntegrityValue)
{
    const auto record = parseInstallRecord("compiler = 'bin/g++'\ningot-integrity = 'sha256:abc'\n");

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->ingotIntegrity, "sha256:abc");
}

TEST(InstallRecordTest, HoldsNoIntegrityValueWhenTheRecordHasNone)
{
    const auto record = parseInstallRecord("compiler = 'bin/g++'\n");

    ASSERT_TRUE(record.has_value());
    EXPECT_FALSE(record->ingotIntegrity.has_value());
}

TEST(InstallRecordTest, RefusesAnIntegrityValueThatIsNotAString)
{
    const auto record = parseInstallRecord("compiler = 'bin/g++'\ningot-integrity = 1\n");

    ASSERT_FALSE(record.has_value());
    EXPECT_NE(record.error().find("ingot-integrity: must be a string"), std::string::npos);
}

TEST(InstallRecordTest, ReadsBackWhatItRenders)
{
    const InstallRecord written{ .compiler = "bin/g++", .runtimeLibraryDirectories = { "lib64", "lib/x86_64-unknown-linux-gnu" }, .ingotIntegrity = "sha256:abc" };

    const auto read = parseInstallRecord(renderInstallRecord(written));

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->compiler, written.compiler);
    EXPECT_EQ(read->runtimeLibraryDirectories, written.runtimeLibraryDirectories);
    EXPECT_EQ(read->ingotIntegrity, written.ingotIntegrity);
}

TEST(InstallRecordTest, RendersNoIntegrityValueWhenThereIsNone)
{
    const InstallRecord written{ .compiler = "bin/g++", .runtimeLibraryDirectories = {} };

    const std::string text = renderInstallRecord(written);

    EXPECT_EQ(text.find("ingot-integrity"), std::string::npos);
    EXPECT_FALSE(parseInstallRecord(text)->ingotIntegrity.has_value());
}
