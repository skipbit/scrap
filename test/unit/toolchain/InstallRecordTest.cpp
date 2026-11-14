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

/**
 * The C compiler, where the toolchain came from and when it was installed
 * are read as written.
 */
TEST(InstallRecordTest, ReadsTheCCompilerTheSourceAndTheTimeOfInstalling)
{
    const auto record = parseInstallRecord("compiler = 'bin/g++'\nc-compiler = 'bin/gcc'\nsource = 'ghcr.io/skipbit/scrap-toolchain/gcc:16.2.0'\ninstalled-at = '2026-11-12T11:37:11Z'\n");

    ASSERT_TRUE(record.has_value()) << record.error();
    EXPECT_EQ(record->cCompiler, std::filesystem::path("bin/gcc"));
    EXPECT_EQ(record->source, "ghcr.io/skipbit/scrap-toolchain/gcc:16.2.0");
    EXPECT_EQ(record->installedAt, "2026-11-12T11:37:11Z");
}

/**
 * A record an earlier scrap wrote holds none of the three, and still reads.
 */
TEST(InstallRecordTest, HoldsNothingAnEarlierRecordDidNotWrite)
{
    const auto record = parseInstallRecord("compiler = 'bin/g++'\nruntime-library-dirs = [ 'lib64' ]\n");

    ASSERT_TRUE(record.has_value()) << record.error();
    EXPECT_FALSE(record->cCompiler.has_value());
    EXPECT_FALSE(record->source.has_value());
    EXPECT_FALSE(record->installedAt.has_value());
}

/**
 * A C compiler outside the toolchain is refused as the C++ compiler is, and
 * the source and the time must be strings.
 */
TEST(InstallRecordTest, RefusesACCompilerOutsideTheToolchainAndValuesThatAreNotStrings)
{
    EXPECT_EQ(parseInstallRecord("compiler = 'bin/g++'\nc-compiler = '../gcc'\n").error(), "line 2: c-compiler: must be a relative path inside the toolchain");
    EXPECT_EQ(parseInstallRecord("compiler = 'bin/g++'\nc-compiler = 1\n").error(), "line 2: c-compiler: must be a string");
    EXPECT_EQ(parseInstallRecord("compiler = 'bin/g++'\nsource = 1\n").error(), "line 2: source: must be a string");
    EXPECT_EQ(parseInstallRecord("compiler = 'bin/g++'\ninstalled-at = 2026-11-12T11:37:11Z\n").error(), "line 2: installed-at: must be a string");
}

TEST(InstallRecordTest, ReadsBackWhatItRenders)
{
    const InstallRecord written{ .compiler = "bin/clang++",
                                 .runtimeLibraryDirectories = { "lib64", "lib/x86_64-unknown-linux-gnu" },
                                 .arguments = { .compile = { "-stdlib=libc++" }, .link = { "-stdlib=libc++", "-fuse-ld=lld" } },
                                 .ingotIntegrity = "sha256:abc",
                                 .cCompiler = "bin/clang",
                                 .source = "https://example.com/llvm.tar.xz",
                                 .installedAt = "2026-11-12T11:37:11Z" };

    const auto read = parseInstallRecord(renderInstallRecord(written));

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->compiler, written.compiler);
    EXPECT_EQ(read->runtimeLibraryDirectories, written.runtimeLibraryDirectories);
    EXPECT_EQ(read->arguments.compile, written.arguments.compile);
    EXPECT_EQ(read->arguments.link, written.arguments.link);
    EXPECT_EQ(read->ingotIntegrity, written.ingotIntegrity);
    EXPECT_EQ(read->cCompiler, written.cCompiler);
    EXPECT_EQ(read->source, written.source);
    EXPECT_EQ(read->installedAt, written.installedAt);
}

/**
 * What is not known is left out, so the record reads as one that did not
 * record it.
 */
TEST(InstallRecordTest, RendersNoCCompilerSourceOrTimeWhenThereAreNone)
{
    const InstallRecord written{ .compiler = "bin/g++", .runtimeLibraryDirectories = {}, .arguments = { .compile = {}, .link = {} } };

    const std::string text = renderInstallRecord(written);

    EXPECT_EQ(text.find("c-compiler"), std::string::npos);
    EXPECT_EQ(text.find("source"), std::string::npos);
    EXPECT_EQ(text.find("installed-at"), std::string::npos);
}

TEST(InstallRecordTest, RendersNoIntegrityValueWhenThereIsNone)
{
    const InstallRecord written{ .compiler = "bin/g++", .runtimeLibraryDirectories = {}, .arguments = { .compile = {}, .link = {} } };

    const std::string text = renderInstallRecord(written);

    EXPECT_EQ(text.find("ingot-integrity"), std::string::npos);
    EXPECT_FALSE(parseInstallRecord(text)->ingotIntegrity.has_value());
}

/**
 * A record of a toolchain that needs nothing more to compile and link, as
 * one of GCC, holds no argument lists, so it reads as the records written
 * before there were any.
 */
TEST(InstallRecordTest, RendersNoArgumentsWhenThereAreNone)
{
    const InstallRecord written{ .compiler = "bin/g++", .runtimeLibraryDirectories = { "lib64" }, .arguments = { .compile = {}, .link = {} } };

    const std::string text = renderInstallRecord(written);

    EXPECT_EQ(text.find("compile-args"), std::string::npos);
    EXPECT_EQ(text.find("link-args"), std::string::npos);
}

/**
 * Compile and link arguments are read in order; a record without them, as
 * one written before there were any, names none.
 */
TEST(InstallRecordTest, ReadsTheCompileAndLinkArguments)
{
    const auto record = parseInstallRecord("compiler = 'bin/clang++'\ncompile-args = ['-stdlib=libc++']\nlink-args = ['-stdlib=libc++', '-lc++abi']\n");

    ASSERT_TRUE(record.has_value()) << record.error();
    EXPECT_EQ(record->arguments.compile, std::vector<std::string>{ "-stdlib=libc++" });
    const std::vector<std::string> link{ "-stdlib=libc++", "-lc++abi" };
    EXPECT_EQ(record->arguments.link, link);

    const auto without = parseInstallRecord("compiler = 'bin/g++'\n");
    ASSERT_TRUE(without.has_value()) << without.error();
    EXPECT_TRUE(without->arguments.compile.empty());
    EXPECT_TRUE(without->arguments.link.empty());
}

/**
 * Arguments that are not a list of strings are refused, naming the key.
 */
TEST(InstallRecordTest, RefusesArgumentsThatAreNotAListOfStrings)
{
    EXPECT_EQ(parseInstallRecord("compiler = 'bin/clang++'\ncompile-args = '-stdlib=libc++'\n").error(),
              "line 2: compile-args: must be an array of strings");
    EXPECT_EQ(parseInstallRecord("compiler = 'bin/clang++'\nlink-args = ['-lc++abi', 1]\n").error(), "line 2: link-args: must be a string");
}
