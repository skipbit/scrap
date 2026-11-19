#include "toolchain/StatusWarning.h"

#include "toolchain/HostEnvironment.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

using namespace scrap::Toolchain;

namespace {

ToolchainVersion version(std::string_view text)
{
    return ToolchainVersion::parse(text).value();
}

InstallingHost linuxHost()
{
    return { .environment = { .platform = "linux", .arch = "x86_64" }, .scrapVersion = version("1.0.0"), .glibcVersion = version("2.31") };
}

Ingot ingotFor(std::string_view arch)
{
    return { .platform = "linux", .arch = std::string{ arch }, .source = UrlSource{ .url = "https://example.com/a", .sha256 = "abc" }, .glibcVersion = std::nullopt };
}

IndexedToolchain toolchain(std::string_view text, const ToolchainStatus status, std::string_view arch = "x86_64")
{
    return { .family = "gcc",
             .version = version(text),
             .status = status,
             .compiler = "g++",
             .cCompiler = "gcc",
             .minScrapVersion = std::nullopt,
             .ingots = { ingotFor(arch) } };
}

std::optional<StatusWarning> warningOf(const ToolchainIndex& index, std::string_view text)
{
    return statusWarningFor(index, "gcc", version(text), linuxHost());
}

}  // anonymous namespace

TEST(StatusWarningTest, WarnsOfNothingForAnActiveVersion)
{
    const ToolchainIndex index{ .toolchains = { toolchain("15.2.0", ToolchainStatus::Active) } };

    EXPECT_FALSE(warningOf(index, "15.2.0").has_value());
}

TEST(StatusWarningTest, WarnsOfNothingForAVersionTheIndexDoesNotList)
{
    const ToolchainIndex index{ .toolchains = { toolchain("15.2.0", ToolchainStatus::Deprecated) } };

    EXPECT_FALSE(warningOf(index, "15.1.0").has_value());
    EXPECT_FALSE(statusWarningFor(index, "llvm", version("15.2.0"), linuxHost()).has_value());
}

TEST(StatusWarningTest, OffersTheNewestActiveVersionInsteadOfADeprecatedOne)
{
    const ToolchainIndex index{ .toolchains = { toolchain("15.2.0", ToolchainStatus::Deprecated),
                                                toolchain("16.1.0", ToolchainStatus::Active),
                                                toolchain("16.2.0", ToolchainStatus::Active),
                                                toolchain("16.3.0", ToolchainStatus::Deprecated),
                                                toolchain("17.1.0", ToolchainStatus::Active, "aarch64") } };

    const auto warning = warningOf(index, "15.2.0");

    ASSERT_TRUE(warning.has_value());
    EXPECT_EQ(warning->name, "gcc@15.2.0");
    EXPECT_EQ(warning->status, ToolchainStatus::Deprecated);
    EXPECT_EQ(warning->alternative, "gcc@16.2.0");
}

TEST(StatusWarningTest, OffersNoOlderVersionInsteadOfADeprecatedOne)
{
    const ToolchainIndex index{ .toolchains = { toolchain("15.2.0", ToolchainStatus::Deprecated), toolchain("14.3.0", ToolchainStatus::Active) } };

    const auto warning = warningOf(index, "15.2.0");

    ASSERT_TRUE(warning.has_value());
    EXPECT_EQ(warning->alternative, std::nullopt);
}

TEST(StatusWarningTest, OffersAnOlderVersionInsteadOfOneNoLongerProvided)
{
    const ToolchainIndex index{ .toolchains = { toolchain("15.2.0", ToolchainStatus::Disabled), toolchain("14.3.0", ToolchainStatus::Active) } };

    const auto warning = warningOf(index, "15.2.0");

    ASSERT_TRUE(warning.has_value());
    EXPECT_EQ(warning->status, ToolchainStatus::Disabled);
    EXPECT_EQ(warning->alternative, "gcc@14.3.0");
}

TEST(StatusWarningTest, OffersNothingWhenNoOtherVersionIsActiveHere)
{
    const ToolchainIndex index{ .toolchains = { toolchain("15.2.0", ToolchainStatus::Disabled), toolchain("16.1.0", ToolchainStatus::Active, "aarch64") } };

    const auto warning = warningOf(index, "15.2.0");

    ASSERT_TRUE(warning.has_value());
    EXPECT_EQ(warning->alternative, std::nullopt);
}

TEST(StatusWarningTest, WarnsOfNothingForAVersionNeedingANewerScrap)
{
    IndexedToolchain unread = toolchain("15.2.0", ToolchainStatus::Disabled);
    unread.minScrapVersion = version("2.0.0");
    const ToolchainIndex index{ .toolchains = { unread } };

    EXPECT_FALSE(warningOf(index, "15.2.0").has_value());
}
