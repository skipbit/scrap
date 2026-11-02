#include "toolchain/HostEnvironment.h"

#include <gtest/gtest.h>

using namespace scrap::Toolchain;

/**
 * Every host scrap is released for is one toolchains are built for.
 */
TEST(HostEnvironmentTest, KnowsTheHost)
{
    const auto host = hostEnvironment();

    ASSERT_TRUE(host.has_value());
    EXPECT_TRUE(host->platform == "linux" || host->platform == "darwin") << host->platform;
    EXPECT_TRUE(host->arch == "x86_64" || host->arch == "aarch64") << host->arch;
}

/**
 * The directory name joins the platform and the architecture with a hyphen.
 */
TEST(HostEnvironmentTest, NamesTheDirectoryOfItsToolchains)
{
    EXPECT_EQ((HostEnvironment{ .platform = "linux", .arch = "x86_64" }.directoryName()), "linux-x86_64");
    EXPECT_EQ((HostEnvironment{ .platform = "darwin", .arch = "aarch64" }.directoryName()), "darwin-aarch64");
}
