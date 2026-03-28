#include <gtest/gtest.h>

#include "command/CommandCatalog.h"

using namespace scrap::Command;

namespace {

auto makeEntry(const std::string& name,
               CommandSource source,
               const std::string& category = "",
               const std::string& description = "") -> CommandEntry
{
    CommandEntry entry;
    entry.spec.name = name;
    entry.spec.description = description;
    entry.spec.category = category;
    entry.source = source;
    entry.createHandler = [](const ParsedOptions&) { return nullptr; };
    return entry;
}

auto makeEntryWithSubs(const std::string& name, CommandSource source, std::vector<CommandEntry> subs) -> CommandEntry
{
    auto entry = makeEntry(name, source);
    entry.subcommands = std::move(subs);
    return entry;
}

}  // namespace

// --- addEntries ---

TEST(CommandCatalogTest, AddEntries_SingleEntry)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("build", CommandSource::Builtin));
    catalog.addEntries(std::move(entries));

    EXPECT_NE(catalog.find("build"), nullptr);
}

TEST(CommandCatalogTest, AddEntries_MultipleEntries)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("build", CommandSource::Builtin));
    entries.push_back(makeEntry("run", CommandSource::Builtin));
    entries.push_back(makeEntry("clean", CommandSource::Builtin));
    catalog.addEntries(std::move(entries));

    EXPECT_NE(catalog.find("build"), nullptr);
    EXPECT_NE(catalog.find("run"), nullptr);
    EXPECT_NE(catalog.find("clean"), nullptr);
}

TEST(CommandCatalogTest, AddEntries_NameCollision_FirstWins)
{
    CommandCatalog catalog;

    std::vector<CommandEntry> first;
    first.push_back(makeEntry("build", CommandSource::Builtin, "", "builtin build"));
    catalog.addEntries(std::move(first));

    std::vector<CommandEntry> second;
    second.push_back(makeEntry("build", CommandSource::External, "", "external build"));
    catalog.addEntries(std::move(second));

    const auto* found = catalog.find("build");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->source, CommandSource::Builtin);
    EXPECT_EQ(found->spec.description, "builtin build");
}

TEST(CommandCatalogTest, AddEntries_MultipleCallsMerge)
{
    CommandCatalog catalog;

    std::vector<CommandEntry> batch1;
    batch1.push_back(makeEntry("build", CommandSource::Builtin));
    catalog.addEntries(std::move(batch1));

    std::vector<CommandEntry> batch2;
    batch2.push_back(makeEntry("lint", CommandSource::External));
    catalog.addEntries(std::move(batch2));

    EXPECT_NE(catalog.find("build"), nullptr);
    EXPECT_NE(catalog.find("lint"), nullptr);
}

// --- find ---

TEST(CommandCatalogTest, Find_TopLevelCommand)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("build", CommandSource::Builtin));
    catalog.addEntries(std::move(entries));

    const auto* found = catalog.find("build");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->spec.name, "build");
}

TEST(CommandCatalogTest, Find_DotSeparatedPath)
{
    CommandCatalog catalog;

    std::vector<CommandEntry> subs;
    subs.push_back(makeEntry("install", CommandSource::Builtin));
    subs.push_back(makeEntry("list", CommandSource::Builtin));

    std::vector<CommandEntry> entries;
    entries.push_back(makeEntryWithSubs("toolchain", CommandSource::Builtin, std::move(subs)));
    catalog.addEntries(std::move(entries));

    const auto* found = catalog.find("toolchain.install");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->spec.name, "install");

    const auto* listFound = catalog.find("toolchain.list");
    ASSERT_NE(listFound, nullptr);
    EXPECT_EQ(listFound->spec.name, "list");
}

TEST(CommandCatalogTest, Find_ParentCommand)
{
    CommandCatalog catalog;

    std::vector<CommandEntry> subs;
    subs.push_back(makeEntry("install", CommandSource::Builtin));

    std::vector<CommandEntry> entries;
    entries.push_back(makeEntryWithSubs("toolchain", CommandSource::Builtin, std::move(subs)));
    catalog.addEntries(std::move(entries));

    const auto* found = catalog.find("toolchain");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->spec.name, "toolchain");
    EXPECT_EQ(found->subcommands.size(), 1);
}

TEST(CommandCatalogTest, Find_NonexistentPath)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("build", CommandSource::Builtin));
    catalog.addEntries(std::move(entries));

    EXPECT_EQ(catalog.find("nonexistent"), nullptr);
}

TEST(CommandCatalogTest, Find_PartialInvalidPath)
{
    CommandCatalog catalog;

    std::vector<CommandEntry> subs;
    subs.push_back(makeEntry("install", CommandSource::Builtin));

    std::vector<CommandEntry> entries;
    entries.push_back(makeEntryWithSubs("toolchain", CommandSource::Builtin, std::move(subs)));
    catalog.addEntries(std::move(entries));

    EXPECT_EQ(catalog.find("toolchain.nonexistent"), nullptr);
}

TEST(CommandCatalogTest, Find_EmptyPath)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("build", CommandSource::Builtin));
    catalog.addEntries(std::move(entries));

    EXPECT_EQ(catalog.find(""), nullptr);
}

TEST(CommandCatalogTest, Find_EmptyCatalog)
{
    CommandCatalog catalog;
    EXPECT_EQ(catalog.find("build"), nullptr);
}

TEST(CommandCatalogTest, Find_MalformedPath_LeadingDot)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("toolchain", CommandSource::Builtin));
    catalog.addEntries(std::move(entries));

    EXPECT_EQ(catalog.find(".toolchain"), nullptr);
}

TEST(CommandCatalogTest, Find_MalformedPath_TrailingDot)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("toolchain", CommandSource::Builtin));
    catalog.addEntries(std::move(entries));

    EXPECT_EQ(catalog.find("toolchain."), nullptr);
}

TEST(CommandCatalogTest, Find_MalformedPath_ConsecutiveDots)
{
    CommandCatalog catalog;

    std::vector<CommandEntry> subs;
    subs.push_back(makeEntry("install", CommandSource::Builtin));

    std::vector<CommandEntry> entries;
    entries.push_back(makeEntryWithSubs("toolchain", CommandSource::Builtin, std::move(subs)));
    catalog.addEntries(std::move(entries));

    EXPECT_EQ(catalog.find("toolchain..install"), nullptr);
}

// --- specs ---

TEST(CommandCatalogTest, Specs_DerivesSubcommandTree)
{
    CommandCatalog catalog;

    std::vector<CommandEntry> subs;
    subs.push_back(makeEntry("install", CommandSource::Builtin, "", "Install toolchain"));
    subs.push_back(makeEntry("list", CommandSource::Builtin, "", "List toolchains"));

    std::vector<CommandEntry> entries;
    entries.push_back(makeEntryWithSubs("toolchain", CommandSource::Builtin, std::move(subs)));
    catalog.addEntries(std::move(entries));

    auto specTree = catalog.specs();
    ASSERT_EQ(specTree.size(), 1);
    EXPECT_EQ(specTree[0].name, "toolchain");
    ASSERT_EQ(specTree[0].subcommands.size(), 2);
    EXPECT_EQ(specTree[0].subcommands[0].name, "install");
    EXPECT_EQ(specTree[0].subcommands[0].description, "Install toolchain");
    EXPECT_EQ(specTree[0].subcommands[1].name, "list");
}

TEST(CommandCatalogTest, Specs_EmptyCatalog)
{
    CommandCatalog catalog;
    auto specTree = catalog.specs();
    EXPECT_TRUE(specTree.empty());
}

TEST(CommandCatalogTest, Specs_PreservesOptions)
{
    CommandCatalog catalog;

    auto entry = makeEntry("build", CommandSource::Builtin, "", "Build project");
    entry.spec.options.named.push_back(OptionDef{.longName = "release",
                                                 .shortName = 'r',
                                                 .type = OptionValueType::Bool,
                                                 .required = false,
                                                 .description = "Build in release mode",
                                                 .defaultValue = std::nullopt,
                                                 .choices = {}});

    std::vector<CommandEntry> entries;
    entries.push_back(std::move(entry));
    catalog.addEntries(std::move(entries));

    auto specTree = catalog.specs();
    ASSERT_EQ(specTree.size(), 1);
    ASSERT_EQ(specTree[0].options.named.size(), 1);
    EXPECT_EQ(specTree[0].options.named[0].longName, "release");
}

// --- helpEntries ---

TEST(CommandCatalogTest, HelpEntries_IncludesSourceAndSubcommands)
{
    CommandCatalog catalog;

    std::vector<CommandEntry> subs;
    subs.push_back(makeEntry("install", CommandSource::Builtin));

    std::vector<CommandEntry> entries;
    entries.push_back(makeEntryWithSubs("toolchain", CommandSource::Builtin, std::move(subs)));
    entries.push_back(makeEntry("lint", CommandSource::External));
    catalog.addEntries(std::move(entries));

    auto helpList = catalog.helpEntries();
    ASSERT_EQ(helpList.size(), 2);

    EXPECT_EQ(helpList[0].spec.name, "toolchain");
    EXPECT_EQ(helpList[0].source, CommandSource::Builtin);
    ASSERT_EQ(helpList[0].spec.subcommands.size(), 1);
    EXPECT_EQ(helpList[0].spec.subcommands[0].name, "install");

    EXPECT_EQ(helpList[1].spec.name, "lint");
    EXPECT_EQ(helpList[1].source, CommandSource::External);
    EXPECT_TRUE(helpList[1].spec.subcommands.empty());
}

TEST(CommandCatalogTest, HelpEntries_EmptyCatalog)
{
    CommandCatalog catalog;
    auto helpList = catalog.helpEntries();
    EXPECT_TRUE(helpList.empty());
}
