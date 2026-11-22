#include "project/FileSelection.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

using namespace scrap::Project;

namespace {

using Paths = std::vector<std::filesystem::path>;

const Paths Covered{ "include/api.h", "src/a.cpp", "src/net/b.cpp", "src/net/c.cpp", "src/network.cpp" };

RequestedPath fileNamed(const char* path)
{
    return RequestedPath{ .path = path, .isDirectory = false };
}

RequestedPath directoryNamed(const char* path)
{
    return RequestedPath{ .path = path, .isDirectory = true };
}

}  // namespace

/**
 * With nothing named, every covered file is selected and nothing is left out.
 */
TEST(FileSelectionTest, SelectsEveryFileWhenNothingIsNamed)
{
    const FileSelection selection = selectFiles(Covered, {});

    EXPECT_EQ(selection.selected, Covered);
    EXPECT_TRUE(selection.leftOut.empty());
}

/**
 * A named file selects itself alone.
 */
TEST(FileSelectionTest, SelectsANamedFile)
{
    const FileSelection selection = selectFiles(Covered, { fileNamed("src/a.cpp") });

    EXPECT_EQ(selection.selected, (Paths{ "src/a.cpp" }));
    EXPECT_TRUE(selection.leftOut.empty());
}

/**
 * A named directory selects the files below it, matched by whole path
 * components: src/net does not select src/network.cpp.
 */
TEST(FileSelectionTest, SelectsTheFilesBelowANamedDirectory)
{
    EXPECT_EQ(selectFiles(Covered, { directoryNamed("src/net") }).selected, (Paths{ "src/net/b.cpp", "src/net/c.cpp" }));
    EXPECT_EQ(selectFiles(Covered, { directoryNamed("src/net/") }).selected, (Paths{ "src/net/b.cpp", "src/net/c.cpp" }));
    EXPECT_EQ(selectFiles(Covered, { directoryNamed("src") }).selected, (Paths{ "src/a.cpp", "src/net/b.cpp", "src/net/c.cpp", "src/network.cpp" }));
    EXPECT_EQ(selectFiles(Covered, { directoryNamed(".") }).selected, Covered);
    EXPECT_TRUE(selectFiles(Covered, { directoryNamed("tools") }).selected.empty());
}

/**
 * Paths are compared once normalized.
 */
TEST(FileSelectionTest, ComparesNormalizedPaths)
{
    const FileSelection selection = selectFiles(Covered, { fileNamed("./src/net/../a.cpp"), directoryNamed("./include/./") });

    EXPECT_EQ(selection.selected, (Paths{ "include/api.h", "src/a.cpp" }));
    EXPECT_TRUE(selection.leftOut.empty());
}

/**
 * A named file the command does not cover is left out and listed, in the
 * order named, while the rest are still selected.
 */
TEST(FileSelectionTest, LeavesOutANamedFileItDoesNotCover)
{
    const FileSelection selection = selectFiles(Covered, { fileNamed("tools/gen.cpp"), fileNamed("src/a.cpp"), fileNamed("README.md") });

    EXPECT_EQ(selection.selected, (Paths{ "src/a.cpp" }));
    EXPECT_EQ(selection.leftOut, (Paths{ "tools/gen.cpp", "README.md" }));
}

/**
 * Paths that overlap, or a path named twice, select or leave out each file
 * once, and the selection stays sorted.
 */
TEST(FileSelectionTest, ListsEachFileOnce)
{
    const FileSelection selection = selectFiles(
        Covered,
        { fileNamed("src/net/c.cpp"), directoryNamed("src/net"), fileNamed("src/a.cpp"), fileNamed("src/a.cpp"), fileNamed("tools/gen.cpp"), fileNamed("./tools/gen.cpp") });

    EXPECT_EQ(selection.selected, (Paths{ "src/a.cpp", "src/net/b.cpp", "src/net/c.cpp" }));
    EXPECT_EQ(selection.leftOut, (Paths{ "tools/gen.cpp" }));
}

/**
 * The covered files may come in any order and hold a file twice: the selection
 * is sorted and holds each once.
 */
TEST(FileSelectionTest, DoesNotRelyOnTheCoveredFilesBeingSorted)
{
    const Paths unsorted{ "src/b.cpp", "src/a.cpp", "src/b.cpp" };

    EXPECT_EQ(selectFiles(unsorted, {}).selected, (Paths{ "src/a.cpp", "src/b.cpp" }));
    EXPECT_EQ(selectFiles(unsorted, { fileNamed("src/a.cpp") }).selected, (Paths{ "src/a.cpp" }));
}
