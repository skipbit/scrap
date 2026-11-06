#include "registry/IngotLocation.h"

#include <gtest/gtest.h>

#include <optional>

using scrap::Registry::locateIngot;

namespace {

constexpr auto Digest = "sha256:70f6e1d0a091e0992d26d9e1b887a104348bf6c64221c704af1ede9b48afd30f";

}  // anonymous namespace

TEST(IngotLocationTest, PutsTheNameOfTheTagBelowThePathOfTheRegistry)
{
    const auto location = locateIngot("ghcr.io/skipbit/scrap-toolchain", "gcc:11.1.0", Digest);

    ASSERT_TRUE(location.has_value());
    EXPECT_EQ(location->origin, "https://ghcr.io");
    EXPECT_EQ(location->repository, "skipbit/scrap-toolchain/gcc");
    EXPECT_EQ(location->manifestUrl(), std::string{ "https://ghcr.io/v2/skipbit/scrap-toolchain/gcc/manifests/" } + Digest);
    EXPECT_EQ(location->blobUrl("sha256:9e75"), "https://ghcr.io/v2/skipbit/scrap-toolchain/gcc/blobs/sha256:9e75");
    EXPECT_EQ(location->pullScope(), "repository:skipbit/scrap-toolchain/gcc:pull");
}

TEST(IngotLocationTest, TakesTheNameOfTheTagAloneWhenTheRegistryHasNoPath)
{
    const auto location = locateIngot("registry.example.com:5000", "llvm:18.1.7", Digest);

    ASSERT_TRUE(location.has_value());
    EXPECT_EQ(location->origin, "https://registry.example.com:5000");
    EXPECT_EQ(location->repository, "llvm");
}

TEST(IngotLocationTest, ReachesThisMachineOverHttp)
{
    EXPECT_EQ(locateIngot("127.0.0.1:5000/toolchains", "gcc:1", Digest)->origin, "http://127.0.0.1:5000");
    EXPECT_EQ(locateIngot("localhost/toolchains", "gcc:1", Digest)->origin, "http://localhost");
    EXPECT_EQ(locateIngot("[::1]:5000/toolchains", "gcc:1", Digest)->origin, "http://[::1]:5000");
    EXPECT_EQ(locateIngot("localhost.example.com/toolchains", "gcc:1", Digest)->origin, "https://localhost.example.com");
}

TEST(IngotLocationTest, RefusesARegistryWithoutAHostOrATagWithoutAName)
{
    EXPECT_EQ(locateIngot("", "gcc:1", Digest), std::nullopt);
    EXPECT_EQ(locateIngot("/toolchains", "gcc:1", Digest), std::nullopt);
    EXPECT_EQ(locateIngot(":5000/toolchains", "gcc:1", Digest), std::nullopt);
    EXPECT_EQ(locateIngot("ghcr.io/toolchains", "gcc", Digest), std::nullopt);
    EXPECT_EQ(locateIngot("ghcr.io/toolchains", ":1", Digest), std::nullopt);
}
