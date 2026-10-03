#include "toolchain/SystemArchiver.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <system_error>

using namespace scrap::Toolchain;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * Create a program that can be run below the temp directory, with @p script
 * as its body.
 */
std::filesystem::path createScript(const TempDirectory& temp, const std::string& relative, const std::string& script)
{
    const std::filesystem::path path = temp.writeFile(relative, "#!/bin/sh\n" + script);
    std::error_code ec;
    std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add, ec);
    if (ec) {
        ADD_FAILURE() << "cannot make " << path << " runnable: " << ec.message();
    }
    return path;
}

/**
 * A compiler that answers -print-prog-name=ar with @p answer, and fails when
 * asked anything else.
 */
std::filesystem::path createCompiler(const TempDirectory& temp, const std::string& answer)
{
    return createScript(temp, "compiler/c++", "[ \"$1\" = -print-prog-name=ar ] || exit 1\nprintf '%s\\n' '" + answer + "'\n");
}

}  // namespace

/**
 * A compiler that answers with a path names the archiver it keeps beside
 * itself, which is used without searching.
 */
TEST(SystemArchiverTest, UsesThePathTheCompilerAnswers)
{
    const TempDirectory temp;
    const auto beside = createScript(temp, "compiler/ar", "exit 0\n");
    createScript(temp, "path/ar", "exit 0\n");
    const auto compiler = createCompiler(temp, beside.string());

    const auto found = findArchiver(compiler, { temp.path() / "path" });

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, beside);
}

/**
 * A compiler that answers with a bare name leaves the archiver to the search
 * paths, and the one found is absolute.
 */
TEST(SystemArchiverTest, LooksForABareNameOnTheSearchPaths)
{
    const TempDirectory temp;
    const auto onPath = createScript(temp, "path/ar", "exit 0\n");
    const auto compiler = createCompiler(temp, "ar");

    const auto found = findArchiver(compiler, { temp.path() / "empty", temp.path() / "path" });

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, onPath);
    EXPECT_TRUE(found->is_absolute());
}

/**
 * A name the search paths do not hold is reported as the compiler gave it.
 */
TEST(SystemArchiverTest, ReportsANameTheSearchPathsDoNotHold)
{
    const TempDirectory temp;
    const auto compiler = createCompiler(temp, "llvm-ar");

    const auto found = findArchiver(compiler, { temp.path() / "path" });

    ASSERT_FALSE(found.has_value());
    EXPECT_EQ(found.error().named, "llvm-ar");
}

/**
 * A path the compiler answers with that cannot be run is reported, not
 * replaced by an archiver of the same name on the search paths.
 */
TEST(SystemArchiverTest, ReportsAPathThatCannotBeRun)
{
    const TempDirectory temp;
    createScript(temp, "path/ar", "exit 0\n");
    const std::string missing = (temp.path() / "compiler/ar").string();
    const auto compiler = createCompiler(temp, missing);

    const auto found = findArchiver(compiler, { temp.path() / "path" });

    ASSERT_FALSE(found.has_value());
    EXPECT_EQ(found.error().named, missing);
}

/**
 * A compiler that cannot be asked is reported as such, and no archiver on the
 * search paths stands in for it.
 */
TEST(SystemArchiverTest, ReportsACompilerThatCannotBeAsked)
{
    const TempDirectory temp;
    createScript(temp, "path/ar", "exit 0\n");
    const auto compiler = createScript(temp, "compiler/c++", "exit 1\n");

    const auto found = findArchiver(compiler, { temp.path() / "path" });

    ASSERT_FALSE(found.has_value());
    EXPECT_FALSE(found.error().named.has_value());
}
