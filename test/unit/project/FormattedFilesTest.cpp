#include "project/FormattedFiles.h"

#include "project/Manifest.h"
#include "project/SourceCollector.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <system_error>
#include <vector>

using namespace scrap::Project;
using scrap::TestSupport::TempDirectory;

namespace {

constexpr const char* SourceText = "int value() { return 0; }\n";

Target executableNamed(const char* name, const char* entryPoint)
{
    return Target{ .kind = TargetKind::Executable, .name = name, .source = entryPoint };
}

Target libraryNamed(const char* name, const char* file)
{
    return Target{ .kind = TargetKind::Library, .name = name, .source = file };
}

using Paths = std::vector<std::filesystem::path>;

}  // namespace

/**
 * Below src/, sources and headers of every recognised extension are covered,
 * sorted, and files of other extensions or spellings are not.
 */
TEST(FormattedFilesTest, CoversTheSourcesAndHeadersBelowSrc)
{
    const TempDirectory temp;
    for (const char* name : { "src/main.cpp", "src/a.cc", "src/b.cxx", "src/c.c", "src/d.h", "src/net/e.hh", "src/net/f.hpp", "src/net/g.hxx" }) {
        temp.writeFile(name, SourceText);
    }
    for (const char* name : { "src/notes.txt", "src/upper.CPP", "src/upper.H", "src/inline.inl", "src/module.ixx" }) {
        temp.writeFile(name, SourceText);
    }

    const auto files = filesToFormat(temp.path(), { executableNamed("app", "src/main.cpp") });

    ASSERT_TRUE(files.has_value());
    EXPECT_EQ(*files,
              (Paths{ "src/a.cc", "src/b.cxx", "src/c.c", "src/d.h", "src/main.cpp", "src/net/e.hh", "src/net/f.hpp", "src/net/g.hxx" }));
}

/**
 * Below include/ and the include-dirs of a target, its own and its public
 * ones, headers are covered and sources are not; a directory no target names
 * is not covered at all.
 */
TEST(FormattedFilesTest, CoversTheHeadersBelowIncludeAndTheIncludeDirs)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    temp.writeFile("include/app/api.hpp", SourceText);
    temp.writeFile("include/app/impl.cpp", SourceText);
    temp.writeFile("third/own/own.h", SourceText);
    temp.writeFile("third/shared/shared.h", SourceText);
    temp.writeFile("tools/gen.cpp", SourceText);
    temp.writeFile("tools/gen.h", SourceText);
    Target library = libraryNamed("core", "");
    library.settings.includeDirectories = { "third/own" };
    library.publicSettings.includeDirectories = { "third/shared/" };

    const auto files = filesToFormat(temp.path(), { executableNamed("app", "src/main.cpp"), library });

    ASSERT_TRUE(files.has_value());
    EXPECT_EQ(*files, (Paths{ "include/app/api.hpp", "src/main.cpp", "third/own/own.h", "third/shared/shared.h" }));
}

/**
 * A file a target names in src is covered wherever it sits and whatever it
 * is called, a library's as well as an executable's, and also when it does
 * not exist, so the tool reports it.
 */
TEST(FormattedFilesTest, CoversTheFilesTargetsName)
{
    const TempDirectory temp;
    temp.writeFile("app/start.cpp", SourceText);
    temp.writeFile("gen/extra.cpp", SourceText);

    const auto files = filesToFormat(temp.path(),
                                     { executableNamed("app", "./app/start.cpp"), executableNamed("tool", "tool/missing.cpp"), libraryNamed("core", "gen/extra.cpp") });

    ASSERT_TRUE(files.has_value());
    EXPECT_EQ(*files, (Paths{ "app/start.cpp", "gen/extra.cpp", "tool/missing.cpp" }));
}

/**
 * Nothing below build/ is covered, even when an include-dirs entry or a
 * named file reaches into it.
 */
TEST(FormattedFilesTest, CoversNothingBelowBuild)
{
    const TempDirectory temp;
    temp.writeFile("root.h", SourceText);
    temp.writeFile("build/debug/generated.h", SourceText);
    temp.writeFile("build/main.cpp", SourceText);
    Target executable = executableNamed("app", "build/main.cpp");
    executable.settings.includeDirectories = { ".", "build/debug" };

    const auto files = filesToFormat(temp.path(), { executable });

    ASSERT_TRUE(files.has_value());
    EXPECT_EQ(*files, (Paths{ "root.h" }));
}

/**
 * A file reached more than once, through overlapping directories or as a
 * named file below src/, is listed once.
 */
TEST(FormattedFilesTest, ListsEachFileOnce)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    temp.writeFile("src/util.h", SourceText);
    temp.writeFile("include/api.h", SourceText);
    Target executable = executableNamed("app", "src/main.cpp");
    executable.settings.includeDirectories = { "src", "include", "./include" };
    executable.publicSettings.includeDirectories = { "src" };

    const auto files = filesToFormat(temp.path(), { executable, executableNamed("other", "src/main.cpp") });

    ASSERT_TRUE(files.has_value());
    EXPECT_EQ(*files, (Paths{ "include/api.h", "src/main.cpp", "src/util.h" }));
}

/**
 * A project without src/, include/ or the directories it names covers no
 * files from them, which is not a failure.
 */
TEST(FormattedFilesTest, CoversNothingFromDirectoriesThatDoNotExist)
{
    const TempDirectory temp;
    Target executable = executableNamed("app", "main.cpp");
    executable.settings.includeDirectories = { "absent" };

    const auto files = filesToFormat(temp.path(), { executable });

    ASSERT_TRUE(files.has_value());
    EXPECT_EQ(*files, (Paths{ "main.cpp" }));
}

/**
 * A directory that exists and cannot be read is reported with its path, so
 * a check does not pass over files it never saw.
 */
TEST(FormattedFilesTest, ReportsADirectoryItCannotRead)
{
    const TempDirectory temp;
    temp.writeFile("src/main.cpp", SourceText);
    const std::filesystem::path locked = temp.makeDirectory("include/locked");
    temp.writeFile("include/locked/hidden.h", SourceText);

    std::error_code ec;
    std::filesystem::permissions(locked, std::filesystem::perms::none, ec);
    ASSERT_FALSE(ec) << ec.message();

    // A user who reads the directory anyway, root among them, cannot observe
    // the failure; the fixture is restored before skipping so it can be removed.
    const std::filesystem::directory_iterator probe(locked, ec);
    if (! ec) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);
        GTEST_SKIP() << "this user reads a directory with no permissions";
    }

    const auto files = filesToFormat(temp.path(), { executableNamed("app", "src/main.cpp") });

    std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);

    ASSERT_FALSE(files.has_value());
    EXPECT_EQ(files.error().directory, locked);
    EXPECT_FALSE(files.error().reason.empty());
}
