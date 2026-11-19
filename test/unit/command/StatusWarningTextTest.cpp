#include "command/StatusWarningText.h"

#include "toolchain/StatusWarning.h"
#include "toolchain/ToolchainIndex.h"

#include <gtest/gtest.h>

#include <optional>

using namespace scrap::Command;
using namespace scrap::Toolchain;

TEST(StatusWarningTextTest, WarnsOfADeprecatedToolchainWithANewerOne)
{
    EXPECT_EQ(renderStatusWarning({ .name = "gcc@15.2.0", .status = ToolchainStatus::Deprecated, .alternative = "gcc@16.2.0" }),
              "warning: 'gcc@15.2.0' is deprecated\n"
              "hint: install a newer version, such as 'gcc@16.2.0'\n");
}

TEST(StatusWarningTextTest, WarnsOfAToolchainNoLongerProvidedWithAnotherOne)
{
    EXPECT_EQ(renderStatusWarning({ .name = "gcc@15.2.0", .status = ToolchainStatus::Disabled, .alternative = "gcc@14.3.0" }),
              "warning: 'gcc@15.2.0' is no longer provided\n"
              "hint: install another version, such as 'gcc@14.3.0'\n");
}

TEST(StatusWarningTextTest, WarnsInOneLineWhenNothingCanBeInstalledInstead)
{
    EXPECT_EQ(renderStatusWarning({ .name = "gcc@15.2.0", .status = ToolchainStatus::Deprecated, .alternative = std::nullopt }), "warning: 'gcc@15.2.0' is deprecated\n");
    EXPECT_EQ(renderStatusWarning({ .name = "gcc@15.2.0", .status = ToolchainStatus::Disabled, .alternative = std::nullopt }), "warning: 'gcc@15.2.0' is no longer provided\n");
}

TEST(StatusWarningTextTest, WarnsOfNothingForAnActiveToolchain)
{
    EXPECT_EQ(renderStatusWarning({ .name = "gcc@16.2.0", .status = ToolchainStatus::Active, .alternative = "gcc@16.3.0" }), "");
}
