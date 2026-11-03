#include "toolchain/HostGlibc.h"

#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <optional>

using scrap::Toolchain::hostGlibcVersion;

TEST(HostGlibcTest, ReportsTheVersionOfAGlibcHostAndNoneElsewhere)
{
#if defined(__linux__)
    ASSERT_TRUE(hostGlibcVersion().has_value());
    EXPECT_GE(*hostGlibcVersion(), *scrap::Toolchain::ToolchainVersion::parse("2.17"));
#else
    EXPECT_FALSE(hostGlibcVersion().has_value());
#endif
}
