#include "command/CommandCatalog.h"

#include <gtest/gtest.h>

using namespace scrap::Command;

namespace {

CommandEntry makeEntry(const std::string& name,
                       CommandSource source,
                       const std::string& category = "",
                       const std::string& description = "")
{
    CommandEntry entry;
    entry.spec.name = name;
    entry.spec.description = description;
    entry.spec.category = category;
    entry.source = source;
    entry.createHandler = [](const ParsedOptions&) {
        return nullptr;
    };
    return entry;
}

CommandEntry makeEntryWithSubs(const std::string& name, CommandSource source, std::vector<CommandEntry> subs)
{
    auto entry = makeEntry(name, source);
    entry.subcommands = std::move(subs);
    return entry;
}

const CommandEntry* topLevel(const CommandCatalog& catalog, const std::string& name)
{
    const std::vector<std::string> words = { name };
    return catalog.match(words).deepest;
}

}  // namespace

// --- addEntries ---

TEST(CommandCatalogTest, AddEntries_SingleEntry)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("build", CommandSource::Builtin));
    catalog.addEntries(std::move(entries));

    EXPECT_NE(topLevel(catalog, "build"), nullptr);
}

TEST(CommandCatalogTest, AddEntries_MultipleEntries)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("build", CommandSource::Builtin));
    entries.push_back(makeEntry("run", CommandSource::Builtin));
    entries.push_back(makeEntry("clean", CommandSource::Builtin));
    catalog.addEntries(std::move(entries));

    EXPECT_NE(topLevel(catalog, "build"), nullptr);
    EXPECT_NE(topLevel(catalog, "run"), nullptr);
    EXPECT_NE(topLevel(catalog, "clean"), nullptr);
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

    const auto* found = topLevel(catalog, "build");
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

    EXPECT_NE(topLevel(catalog, "build"), nullptr);
    EXPECT_NE(topLevel(catalog, "lint"), nullptr);
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
    entry.spec.options.named.push_back(OptionDef{ .longName = "release",
                                                  .shortName = 'r',
                                                  .type = OptionValueType::Bool,
                                                  .required = false,
                                                  .description = "Build in release mode",
                                                  .defaultValue = std::nullopt,
                                                  .choices = {} });

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

// --- match ---

namespace {

CommandCatalog makeToolchainCatalog()
{
    std::vector<CommandEntry> subs;
    subs.push_back(makeEntry("install", CommandSource::Builtin));

    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("build", CommandSource::Builtin));
    entries.push_back(makeEntryWithSubs("toolchain", CommandSource::Builtin, std::move(subs)));

    CommandCatalog catalog;
    catalog.addEntries(std::move(entries));
    return catalog;
}

}  // namespace

TEST(CommandCatalogTest, Match_TopLevel)
{
    const auto catalog = makeToolchainCatalog();
    const std::vector<std::string> words = { "build" };

    const auto found = catalog.match(words);
    ASSERT_NE(found.deepest, nullptr);
    EXPECT_EQ(found.deepest->spec.name, "build");
    EXPECT_EQ(found.depth, 1);
}

TEST(CommandCatalogTest, Match_Nested)
{
    const auto catalog = makeToolchainCatalog();
    const std::vector<std::string> words = { "toolchain", "install" };

    const auto found = catalog.match(words);
    ASSERT_NE(found.deepest, nullptr);
    EXPECT_EQ(found.deepest->spec.name, "install");
    EXPECT_EQ(found.depth, 2);
}

TEST(CommandCatalogTest, Match_PartialPath)
{
    const auto catalog = makeToolchainCatalog();
    const std::vector<std::string> words = { "toolchain", "nonexistent", "install" };

    const auto found = catalog.match(words);
    ASSERT_NE(found.deepest, nullptr);
    EXPECT_EQ(found.deepest->spec.name, "toolchain");
    EXPECT_EQ(found.depth, 1);
}

TEST(CommandCatalogTest, Match_NoFirstWord)
{
    const auto catalog = makeToolchainCatalog();
    const std::vector<std::string> words = { "nonexistent", "install" };

    const auto found = catalog.match(words);
    EXPECT_EQ(found.deepest, nullptr);
    EXPECT_EQ(found.depth, 0);
}

TEST(CommandCatalogTest, Match_EmptyWords)
{
    const auto catalog = makeToolchainCatalog();

    const auto found = catalog.match({});
    EXPECT_EQ(found.deepest, nullptr);
    EXPECT_EQ(found.depth, 0);
}

TEST(CommandCatalogTest, Match_NameWithADot)
{
    CommandCatalog catalog;
    std::vector<CommandEntry> entries;
    entries.push_back(makeEntry("foo.sh", CommandSource::External));
    catalog.addEntries(std::move(entries));
    const std::vector<std::string> words = { "foo.sh" };

    const auto found = catalog.match(words);
    ASSERT_NE(found.deepest, nullptr);
    EXPECT_EQ(found.deepest->spec.name, "foo.sh");
    EXPECT_EQ(found.depth, 1);
}

TEST(CommandCatalogTest, Match_NestedNameIsNotTopLevel)
{
    const auto catalog = makeToolchainCatalog();
    const std::vector<std::string> words = { "install" };

    const auto found = catalog.match(words);
    EXPECT_EQ(found.deepest, nullptr);
    EXPECT_EQ(found.depth, 0);
}
