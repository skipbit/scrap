#include "command/IndexUpdateReport.h"

#include "toolchain/IndexChanges.h"
#include "toolchain/ToolchainIndex.h"

#include <gtest/gtest.h>

#include <optional>

using namespace scrap::Command;
using scrap::Toolchain::ChangedToolchain;
using scrap::Toolchain::IndexChanges;
using scrap::Toolchain::ToolchainStatus;

TEST(IndexUpdateReportTest, ListsWhatIsNewAndWhatChanged)
{
    const IndexChanges changes{ .added = { "gcc@16.3.0", "llvm@23.1.0" },
                                .changed = { ChangedToolchain{ .name = "gcc@15.2.0", .before = ToolchainStatus::Active, .after = ToolchainStatus::Deprecated },
                                             ChangedToolchain{ .name = "llvm@20.1.10", .before = ToolchainStatus::Deprecated, .after = ToolchainStatus::Disabled } } };

    EXPECT_EQ(renderIndexChanges("2026-11-15T10:00:00Z", "linux-x86_64", changes),
              "fetched the toolchain index at 2026-11-15T10:00:00Z\n"
              "new:\n"
              "  gcc@16.3.0\n"
              "  llvm@23.1.0\n"
              "changed:\n"
              "  gcc@15.2.0    active -> deprecated\n"
              "  llvm@20.1.10  deprecated -> disabled\n");
}

TEST(IndexUpdateReportTest, LeavesOutAKindOfChangeThereIsNoneOf)
{
    EXPECT_EQ(renderIndexChanges("2026-11-15T10:00:00Z", "linux-x86_64", IndexChanges{ .added = { "gcc@16.3.0" }, .changed = {} }),
              "fetched the toolchain index at 2026-11-15T10:00:00Z\n"
              "new:\n"
              "  gcc@16.3.0\n");
    EXPECT_EQ(renderIndexChanges("2026-11-15T10:00:00Z",
                                 "linux-x86_64",
                                 IndexChanges{ .added = {}, .changed = { ChangedToolchain{ .name = "gcc@15.2.0", .before = ToolchainStatus::Disabled, .after = ToolchainStatus::Active } } }),
              "fetched the toolchain index at 2026-11-15T10:00:00Z\n"
              "changed:\n"
              "  gcc@15.2.0  disabled -> active\n");
}

TEST(IndexUpdateReportTest, SaysOnOneLineThatNothingChanged)
{
    EXPECT_EQ(renderIndexChanges("2026-11-15T10:00:00Z", "darwin-aarch64", IndexChanges{}),
              "fetched the toolchain index at 2026-11-15T10:00:00Z; nothing has changed for darwin-aarch64\n");
}

TEST(IndexUpdateReportTest, CountsWhatTheHostCanInstallFromTheFirstIndex)
{
    EXPECT_EQ(renderFirstIndex("2026-11-15T10:00:00Z", "linux-x86_64", 40), "fetched the toolchain index at 2026-11-15T10:00:00Z: 40 toolchains for linux-x86_64\n");
    EXPECT_EQ(renderFirstIndex("2026-11-15T10:00:00Z", "linux-x86_64", 1), "fetched the toolchain index at 2026-11-15T10:00:00Z: 1 toolchain for linux-x86_64\n");
}

TEST(IndexUpdateReportTest, SaysTheCopyIsKeptWhenTheIndexCannotBeFetched)
{
    EXPECT_EQ(renderCopyKept("the server answered with status 500", "2026-11-14T09:00:00Z"),
              "error: cannot fetch the toolchain index: the server answered with status 500\n"
              "hint: the copy fetched at 2026-11-14T09:00:00Z is kept as it was\n");
}

TEST(IndexUpdateReportTest, SaysWhenTheFetchTimeIsKnownOrNot)
{
    EXPECT_EQ(fetchTimeText("2026-11-15T10:00:00Z"), "2026-11-15T10:00:00Z");
    EXPECT_EQ(fetchTimeText(std::nullopt), "an unknown time");
}
