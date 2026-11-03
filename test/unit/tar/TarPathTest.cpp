#include "tar/TarPath.h"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

using scrap::Tar::memberComponents;
using scrap::Tar::symbolicLinkStaysInside;

namespace {

using Components = std::vector<std::string_view>;

}  // anonymous namespace

TEST(TarPathTest, SplitsAPathIntoComponents)
{
    EXPECT_EQ(memberComponents("a/b/c"), (Components{ "a", "b", "c" }));
}

TEST(TarPathTest, DropsDotsAndEmptyComponents)
{
    EXPECT_EQ(memberComponents("./a//b/./c/"), (Components{ "a", "b", "c" }));
}

TEST(TarPathTest, TakesTheRootOfTheArchiveAsNoComponents)
{
    EXPECT_EQ(memberComponents("./"), Components{});
    EXPECT_EQ(memberComponents("."), Components{});
}

TEST(TarPathTest, KeepsNamesThatOnlyLookLikeDots)
{
    EXPECT_EQ(memberComponents("a/..b/c../..."), (Components{ "a", "..b", "c..", "..." }));
}

TEST(TarPathTest, RefusesAnEmptyPath)
{
    EXPECT_FALSE(memberComponents(""));
}

TEST(TarPathTest, RefusesAnAbsolutePath)
{
    EXPECT_FALSE(memberComponents("/etc/passwd"));
}

TEST(TarPathTest, RefusesAParentDirectoryAnywhere)
{
    EXPECT_FALSE(memberComponents(".."));
    EXPECT_FALSE(memberComponents("../a"));
    EXPECT_FALSE(memberComponents("a/../b"));
    EXPECT_FALSE(memberComponents("a/.."));
    EXPECT_FALSE(memberComponents("./../a"));
}

TEST(TarPathTest, AcceptsALinkBesideItself)
{
    const Components link{ "dir", "link" };
    EXPECT_TRUE(symbolicLinkStaysInside(link, "file"));
    EXPECT_TRUE(symbolicLinkStaysInside(link, "./file"));
    EXPECT_TRUE(symbolicLinkStaysInside(link, "sub/file"));
}

TEST(TarPathTest, AcceptsALinkUpToTheRootAndNoFurther)
{
    const Components link{ "a", "b", "link" };
    EXPECT_TRUE(symbolicLinkStaysInside(link, ".."));
    EXPECT_TRUE(symbolicLinkStaysInside(link, "../.."));
    EXPECT_TRUE(symbolicLinkStaysInside(link, "../../sibling/file"));
    EXPECT_FALSE(symbolicLinkStaysInside(link, "../../.."));
    EXPECT_FALSE(symbolicLinkStaysInside(link, "../../../outside"));
}

TEST(TarPathTest, RefusesALinkAtTheRootGoingUp)
{
    const Components link{ "link" };
    EXPECT_TRUE(symbolicLinkStaysInside(link, "file"));
    EXPECT_FALSE(symbolicLinkStaysInside(link, ".."));
}

TEST(TarPathTest, RefusesAnAbsoluteOrEmptyTarget)
{
    const Components link{ "dir", "link" };
    EXPECT_FALSE(symbolicLinkStaysInside(link, "/etc/passwd"));
    EXPECT_FALSE(symbolicLinkStaysInside(link, ""));
}

TEST(TarPathTest, RefusesAParentDirectoryAfterAName)
{
    const Components link{ "a", "b", "link" };
    EXPECT_FALSE(symbolicLinkStaysInside(link, "name/.."));
    EXPECT_FALSE(symbolicLinkStaysInside(link, "../name/../other"));
}

TEST(TarPathTest, RefusesALinkWithNoName)
{
    EXPECT_FALSE(symbolicLinkStaysInside({}, "file"));
}
