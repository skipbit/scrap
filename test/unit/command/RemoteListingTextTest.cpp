#include "command/RemoteListingText.h"

#include "command/RemoteListing.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainIndex.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

using namespace scrap::Command;
using namespace scrap::Toolchain;

namespace {

std::vector<RemoteToolchain> rows()
{
    return { { .name = "gcc@16.2.0", .status = ToolchainStatus::Active, .installed = false, .barrier = std::nullopt },
             { .name = "gcc@15.3.0", .status = ToolchainStatus::Disabled, .installed = false, .barrier = NoLongerOffered{ .family = "gcc", .alternative = std::nullopt } },
             { .name = "gcc@15.2.0", .status = ToolchainStatus::Deprecated, .installed = true, .barrier = std::nullopt },
             { .name = "llvm@17.0.1", .status = ToolchainStatus::Active, .installed = false, .barrier = NoIngotForHost{ .environment = "linux-x86_64", .versions = {} } } };
}

}  // anonymous namespace

TEST(RemoteListingTextTest, ListsTheToolchainsAndWhenTheIndexWasFetched)
{
    std::vector<RemoteToolchain> installable = rows();
    std::erase_if(installable, [](const RemoteToolchain& row) {
        return row.barrier.has_value();
    });

    EXPECT_EQ(renderRemoteListingTable(installable, "2026-11-17T06:06:44Z", false),
              "TOOLCHAIN   STATUS      INSTALLED\n"
              "gcc@16.2.0  active\n"
              "gcc@15.2.0  deprecated  yes\n"
              "\n"
              "index fetched at 2026-11-17T06:06:44Z\n");
}

TEST(RemoteListingTextTest, NotesWhyTheHostCannotInstallAVersionWhenListingAll)
{
    EXPECT_EQ(renderRemoteListingTable(rows(), std::nullopt, true),
              "TOOLCHAIN    STATUS      INSTALLED  NOTE\n"
              "gcc@16.2.0   active\n"
              "gcc@15.3.0   disabled               no longer provided\n"
              "gcc@15.2.0   deprecated  yes\n"
              "llvm@17.0.1  active                 not built for linux-x86_64\n"
              "\n"
              "index fetched at an unknown time\n");
}

TEST(RemoteListingTextTest, SaysWhatToDoWhenNothingIsListed)
{
    EXPECT_EQ(renderNothingInstallableHere("linux-x86_64"),
              "no toolchain in the index can be installed on linux-x86_64\n"
              "hint: run 'scrap toolchain list --remote --all' to list them all\n");
    EXPECT_EQ(renderIndexNotFetched(),
              "error: the toolchain index has not been fetched\n"
              "hint: run 'scrap toolchain update' to fetch it\n");
    EXPECT_EQ(renderAllWithoutRemote(),
              "error: '--all' is used with '--remote'\n"
              "hint: run 'scrap toolchain list --remote --all'\n");
}
