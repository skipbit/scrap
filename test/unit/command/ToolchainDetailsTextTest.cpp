#include "command/ToolchainDetailsText.h"

#include "command/ToolchainDetails.h"
#include "command/ToolchainNaming.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/SystemCompiler.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace scrap::Command;
using namespace scrap::Toolchain;

namespace {

InstallRecord fullRecord()
{
    return InstallRecord{ .compiler = "bin/g++",
                          .runtimeLibraryDirectories = { "lib64" },
                          .arguments = { .compile = {}, .link = { "-static-libstdc++" } },
                          .ingotIntegrity = "sha256:abc",
                          .cCompiler = "bin/gcc",
                          .source = "ghcr.io/example/gcc:15.2.0",
                          .installedAt = "2026-11-12T11:37:11Z" };
}

InstalledDetails installed(InstallRecord record)
{
    return InstalledDetails{ .name = "gcc@15.2.0",
                             .directory = "/home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64",
                             .record = std::move(record),
                             .selection = { .isDefault = true, .usedHere = ChoiceSource::Project, .inProject = true } };
}

IndexedToolchain indexed(const ToolchainStatus status)
{
    return IndexedToolchain{ .family = "llvm",
                             .version = ToolchainVersion::parse("22.1.0").value(),
                             .status = status,
                             .compiler = "clang++",
                             .cCompiler = "clang",
                             .minScrapVersion = std::nullopt,
                             .ingots = {} };
}

Ingot registryIngot()
{
    return Ingot{ .platform = "linux",
                  .arch = "x86_64",
                  .source = RegistrySource{ .registry = "ghcr.io/example", .tag = "llvm:22.1.0", .digest = "sha256:def" },
                  .glibcVersion = std::nullopt };
}

}  // anonymous namespace

TEST(ToolchainDetailsTextTest, TellsAnInstalledToolchainWithItsPathsInFull)
{
    EXPECT_EQ(renderToolchainDetails(installed(fullRecord())),
              "name:               gcc@15.2.0\n"
              "installed:          yes\n"
              "directory:          /home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64\n"
              "source:             ghcr.io/example/gcc:15.2.0\n"
              "integrity:          sha256:abc\n"
              "installed at:       2026-11-12T11:37:11Z\n"
              "compiler:           /home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64/bin/g++\n"
              "C compiler:         /home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64/bin/gcc\n"
              "runtime libraries:  /home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64/lib64\n"
              "compile arguments:  (none)\n"
              "link arguments:     -static-libstdc++\n"
              "default:            yes\n"
              "used here:          from scrap.toml\n");
}

TEST(ToolchainDetailsTextTest, TellsWhatAnEarlierScrapDidNotRecord)
{
    InstallRecord record = fullRecord();
    record.source = std::nullopt;
    record.installedAt = std::nullopt;
    record.ingotIntegrity = std::nullopt;
    record.cCompiler = std::nullopt;
    const InstalledDetails details = installed(record);

    const std::string text = renderToolchainDetails(details);

    EXPECT_NE(text.find("source:             not recorded\n"), std::string::npos);
    EXPECT_NE(text.find("integrity:          not recorded\n"), std::string::npos);
    EXPECT_NE(text.find("installed at:       not recorded\n"), std::string::npos);
    EXPECT_EQ(text.find("C compiler"), std::string::npos);
    EXPECT_EQ(renderUnrecordedHint(details), "hint: reinstall it with 'scrap toolchain install --force gcc@15.2.0' to record them\n");
}

TEST(ToolchainDetailsTextTest, GivesNoHintWhenTheRecordTellsWhereAndWhen)
{
    EXPECT_FALSE(renderUnrecordedHint(installed(fullRecord())).has_value());
}

TEST(ToolchainDetailsTextTest, PutsEachRuntimeLibraryDirectoryOnALineOfItsOwn)
{
    InstallRecord record = fullRecord();
    record.runtimeLibraryDirectories = { "lib", "lib64" };
    record.arguments.compile = { "-nostdinc++", "-isystem", "include/c++/v1" };

    const std::string text = renderToolchainDetails(installed(record));

    EXPECT_NE(text.find("runtime libraries:  /home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64/lib\n"
                        "                    /home/u/.scrap/toolchains/gcc/15.2.0/linux-x86_64/lib64\n"),
              std::string::npos);
    EXPECT_NE(text.find("compile arguments:  -nostdinc++ -isystem include/c++/v1\n"), std::string::npos);
}

TEST(ToolchainDetailsTextTest, LeavesOutWhereItIsUsedOutsideAProject)
{
    InstalledDetails details = installed(fullRecord());
    details.selection = { .isDefault = false, .usedHere = std::nullopt, .inProject = false };

    const std::string text = renderToolchainDetails(details);

    EXPECT_NE(text.find("default:            no\n"), std::string::npos);
    EXPECT_EQ(text.find("used here"), std::string::npos);
}

TEST(ToolchainDetailsTextTest, SaysAToolchainIsNotUsedHereInAProject)
{
    InstalledDetails details = installed(fullRecord());
    details.selection.usedHere = std::nullopt;

    EXPECT_NE(renderToolchainDetails(details).find("used here:          no\n"), std::string::npos);
}

TEST(ToolchainDetailsTextTest, SaysItIsUnknownWhetherAToolchainIsUsedHereWhenTheNameCannotBeRead)
{
    InstalledDetails details = installed(fullRecord());
    details.selection = { .isDefault = false, .usedHere = std::nullopt, .inProject = true, .hereUnreadable = true };

    EXPECT_NE(renderToolchainDetails(details).find("used here:          unknown\n"), std::string::npos);
}

TEST(ToolchainDetailsTextTest, TellsAToolchainNotInstalledAsTheIndexTellsIt)
{
    const IndexedDetails details{ .lookup = { .toolchain = indexed(ToolchainStatus::Active), .ingot = registryIngot(), .barrier = std::nullopt },
                                  .fetchedAt = "2026-11-16T10:09:01Z" };

    EXPECT_EQ(renderToolchainDetails(details),
              "name:              llvm@22.1.0\n"
              "installed:         no\n"
              "status:            active\n"
              "compiler:          clang++\n"
              "C compiler:        clang\n"
              "source:            ghcr.io/example/llvm:22.1.0\n"
              "integrity:         sha256:def\n"
              "index fetched at:  2026-11-16T10:09:01Z\n");
}

TEST(ToolchainDetailsTextTest, TellsWhyAToolchainCannotBeInstalledHere)
{
    const IndexedDetails details{ .lookup = { .toolchain = indexed(ToolchainStatus::Disabled),
                                              .ingot = std::nullopt,
                                              .barrier = NoIngotForHost{ .environment = "linux-x86_64", .versions = {} } },
                                  .fetchedAt = std::nullopt };

    const std::string text = renderToolchainDetails(details);

    EXPECT_NE(text.find("status:                    disabled\n"), std::string::npos);
    EXPECT_NE(text.find("cannot be installed here:  not built for linux-x86_64\n"), std::string::npos);
    EXPECT_EQ(text.find("source"), std::string::npos);
    EXPECT_EQ(text.find("integrity"), std::string::npos);
    EXPECT_NE(text.find("index fetched at:          unknown\n"), std::string::npos);
}

TEST(ToolchainDetailsTextTest, TellsEachReasonAToolchainCannotBeInstalled)
{
    IndexedDetails details{ .lookup = { .toolchain = indexed(ToolchainStatus::Disabled), .ingot = registryIngot(), .barrier = NoLongerOffered{ .family = "llvm", .alternative = std::nullopt } },
                            .fetchedAt = std::nullopt };
    EXPECT_NE(renderToolchainDetails(details).find(":  no longer provided\n"), std::string::npos);

    details.lookup.barrier = NeedsNewerGlibc{ .required = "2.34", .host = "2.31" };
    EXPECT_NE(renderToolchainDetails(details).find(":  needs glibc 2.34 or later; this host has 2.31\n"), std::string::npos);

    details.lookup.barrier = NeedsNewerScrap{ .required = "2.0.0" };
    EXPECT_NE(renderToolchainDetails(details).find(":  needs scrap 2.0.0 or later\n"), std::string::npos);
}

TEST(ToolchainDetailsTextTest, TellsTheSystemCompilerAndHowItWasFound)
{
    const SystemDetails details{ .compiler = { .path = "/usr/bin/g++", .origin = CompilerOrigin::KnownName },
                                 .selection = { .isDefault = false, .usedHere = CompilerOrigin::KnownName, .inProject = true } };

    EXPECT_EQ(renderToolchainDetails(details),
              "name:       system\n"
              "found:      yes\n"
              "compiler:   /usr/bin/g++ (found on PATH as a known name)\n"
              "default:    no\n"
              "used here:  found on PATH as a known name\n");
}
