#include <gtest/gtest.h>

#include "toolchain/CompilerIdentity.h"

#include "support/TempDirectory.h"

#include <filesystem>
#include <string>
#include <system_error>

using namespace scrap::Toolchain;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * Create a program below the temp directory that runs @p body.
 */
auto createCompiler(const TempDirectory& temp, const std::string& body) -> std::filesystem::path
{
    const std::filesystem::path path = temp.writeFile("bin/c++", "#!/bin/sh\n" + body + "\n");
    std::error_code ec;
    std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add, ec);
    if (ec) {
        ADD_FAILURE() << "cannot make " << path << " runnable: " << ec.message();
    }
    return path;
}

}  // namespace

/**
 * gcc is told by __GNUC__ and versioned by it.
 */
TEST(CompilerIdentityTest, ReadsGcc)
{
    const auto identity = readCompilerIdentity("#define __GNUC__ 13\n"
                                               "#define __GNUC_MINOR__ 3\n"
                                               "#define __GNUC_PATCHLEVEL__ 0\n");

    EXPECT_EQ(identity.family, CompilerFamily::Gcc);
    EXPECT_EQ(identity.version.major, 13);
    EXPECT_EQ(identity.version.minor, 3);
    EXPECT_EQ(identity.version.patch, 0);
}

/**
 * clang defines __GNUC__ as well, as 4, and is still told apart as clang.
 */
TEST(CompilerIdentityTest, ReadsClangThoughItDefinesGnuc)
{
    const auto identity = readCompilerIdentity("#define __GNUC__ 4\n"
                                               "#define __GNUC_MINOR__ 2\n"
                                               "#define __clang__ 1\n"
                                               "#define __clang_major__ 22\n"
                                               "#define __clang_minor__ 1\n"
                                               "#define __clang_patchlevel__ 8\n");

    EXPECT_EQ(identity.family, CompilerFamily::Clang);
    EXPECT_EQ(identity.version.major, 22);
    EXPECT_EQ(identity.version.minor, 1);
    EXPECT_EQ(identity.version.patch, 8);
}

/**
 * AppleClang defines what clang does and is told apart by its build macro,
 * keeping the version Xcode gives it.
 */
TEST(CompilerIdentityTest, ReadsAppleClangThoughItDefinesClang)
{
    const auto identity = readCompilerIdentity("#define __GNUC__ 4\n"
                                               "#define __apple_build_version__ 15000309\n"
                                               "#define __clang__ 1\n"
                                               "#define __clang_major__ 15\n"
                                               "#define __clang_minor__ 0\n"
                                               "#define __clang_patchlevel__ 0\n");

    EXPECT_EQ(identity.family, CompilerFamily::AppleClang);
    EXPECT_EQ(identity.version.major, 15);
}

/**
 * A compiler defining none of the macros read is unknown.
 */
TEST(CompilerIdentityTest, LeavesOtherCompilersUnknown)
{
    EXPECT_EQ(readCompilerIdentity("").family, CompilerFamily::Unknown);
    EXPECT_EQ(readCompilerIdentity("#define __INTEL_COMPILER 2021\n").family, CompilerFamily::Unknown);
}

/**
 * A macro is matched by its whole name, not by a name it starts with.
 */
TEST(CompilerIdentityTest, MatchesAMacroByItsWholeName)
{
    EXPECT_EQ(readCompilerIdentity("#define __GNUC_MINOR__ 3\n").family, CompilerFamily::Unknown);
    EXPECT_EQ(readCompilerIdentity("#define __clang_major__ 22\n").family, CompilerFamily::Unknown);
    // A macro whose name begins with one that is read is another macro.
    EXPECT_EQ(readCompilerIdentity("#define __clang__x 1\n").family, CompilerFamily::Unknown);
    // A macro with no value is not read as one that has a value.
    EXPECT_EQ(readCompilerIdentity("#define __clang__\n").family, CompilerFamily::Unknown);
}

/**
 * Versions compare by major number first, then by minor.
 */
TEST(CompilerIdentityTest, ComparesVersions)
{
    const CompilerVersion version{.major = 11, .minor = 1, .patch = 0};

    EXPECT_TRUE(isAtLeast(version, 11, 1));
    EXPECT_TRUE(isAtLeast(version, 11));
    EXPECT_TRUE(isAtLeast(version, 10, 9));
    EXPECT_FALSE(isAtLeast(version, 11, 2));
    EXPECT_FALSE(isAtLeast(version, 12));
}

/**
 * The compiler is asked by running it to print its predefined macros.
 */
TEST(CompilerIdentityTest, AsksTheCompilerForItsMacros)
{
    const TempDirectory temp;
    const auto compiler = createCompiler(temp,
                                         "[ \"$*\" = \"-dM -E -x c++ /dev/null\" ] || exit 1\n"
                                         "printf '#define __GNUC__ 14\\n#define __GNUC_MINOR__ 2\\n'");

    const auto identity = identifyCompiler(compiler);

    EXPECT_EQ(identity.family, CompilerFamily::Gcc);
    EXPECT_EQ(identity.version.major, 14);
    EXPECT_EQ(identity.version.minor, 2);
}

/**
 * A compiler that fails to answer is unknown rather than an error.
 */
TEST(CompilerIdentityTest, LeavesACompilerThatFailsUnknown)
{
    const TempDirectory temp;
    const auto compiler = createCompiler(temp, "printf '#define __GNUC__ 14\\n'; exit 1");

    EXPECT_EQ(identifyCompiler(compiler).family, CompilerFamily::Unknown);
}

/**
 * A compiler that cannot be run is unknown rather than an error.
 */
TEST(CompilerIdentityTest, LeavesACompilerThatCannotBeRunUnknown)
{
    const TempDirectory temp;

    EXPECT_EQ(identifyCompiler(temp.path() / "absent").family, CompilerFamily::Unknown);
}
