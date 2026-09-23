#include "command/BuiltinCommandResolver.h"

#include "command/CommandCatalog.h"
#include "project/driver/DiskProjectFileSystem.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace scrap::Command;

namespace {

/// File system the resolver hands to the new command handler.
scrap::Project::DiskProjectFileSystem& diskFileSystem()
{
    static scrap::Project::DiskProjectFileSystem files;
    return files;
}

/**
 * Minimal HelpRenderer mock for BuiltinCommandResolver DI.
 */
class MockHelpRenderer : public HelpRenderer {
public:
    /**
     * Return a fixed global help string.
     */
    std::string renderGlobal([[maybe_unused]] std::span<const HelpEntry> entries) const override
    {
        return "global help";
    }

    /**
     * Return a fixed command help string.
     */
    std::string renderCommand([[maybe_unused]] const CommandSpec& spec) const override
    {
        return "command help";
    }
};

/**
 * Minimal VersionRenderer mock for BuiltinCommandResolver DI.
 */
class MockVersionRenderer : public VersionRenderer {
public:
    /**
     * Return a fixed version string.
     */
    std::string render() const override
    {
        return "scrap 0.0.1-test";
    }
};

/**
 * Find an entry by name in a flat vector.
 */
const CommandEntry* findByName(const std::vector<CommandEntry>& entries, const std::string& name)
{
    auto it = std::ranges::find_if(entries, [&](const CommandEntry& e) {
        return e.spec.name == name;
    });
    return it != entries.end() ? &(*it) : nullptr;
}

}  // namespace

/**
 * Verify that help and version entries are always present.
 */
TEST(BuiltinCommandResolverTest, ReturnsHelpAndVersion)
{
    MockHelpRenderer helpRenderer;
    MockVersionRenderer versionRenderer;
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer, diskFileSystem());

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    EXPECT_NE(findByName(entries, "help"), nullptr);
    EXPECT_NE(findByName(entries, "version"), nullptr);
}

/**
 * Verify that placeholder entries for all domain commands are present.
 */
TEST(BuiltinCommandResolverTest, ReturnsPlaceholderCommands)
{
    MockHelpRenderer helpRenderer;
    MockVersionRenderer versionRenderer;
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer, diskFileSystem());

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    EXPECT_NE(findByName(entries, "new"), nullptr);
    EXPECT_NE(findByName(entries, "build"), nullptr);
    EXPECT_NE(findByName(entries, "run"), nullptr);
    EXPECT_NE(findByName(entries, "clean"), nullptr);
    EXPECT_NE(findByName(entries, "toolchain"), nullptr);
    EXPECT_NE(findByName(entries, "template"), nullptr);
}

/**
 * Verify that toolchain has install/list/select subcommands.
 */
TEST(BuiltinCommandResolverTest, ToolchainHasSubcommands)
{
    MockHelpRenderer helpRenderer;
    MockVersionRenderer versionRenderer;
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer, diskFileSystem());

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    const auto* toolchain = findByName(entries, "toolchain");
    ASSERT_NE(toolchain, nullptr);
    EXPECT_EQ(toolchain->subcommands.size(), 3);
    EXPECT_NE(findByName(toolchain->subcommands, "list"), nullptr);
    EXPECT_NE(findByName(toolchain->subcommands, "install"), nullptr);
    EXPECT_NE(findByName(toolchain->subcommands, "select"), nullptr);
}

/**
 * Verify that template has list/update subcommands.
 */
TEST(BuiltinCommandResolverTest, TemplateHasSubcommands)
{
    MockHelpRenderer helpRenderer;
    MockVersionRenderer versionRenderer;
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer, diskFileSystem());

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    const auto* tmpl = findByName(entries, "template");
    ASSERT_NE(tmpl, nullptr);
    EXPECT_EQ(tmpl->subcommands.size(), 2);
    EXPECT_NE(findByName(tmpl->subcommands, "list"), nullptr);
    EXPECT_NE(findByName(tmpl->subcommands, "update"), nullptr);
}

/**
 * Verify that all entries from BuiltinCommandResolver have Builtin source.
 */
TEST(BuiltinCommandResolverTest, AllEntriesAreBuiltinSource)
{
    MockHelpRenderer helpRenderer;
    MockVersionRenderer versionRenderer;
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer, diskFileSystem());

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    for (const auto& entry : entries) {
        EXPECT_EQ(entry.source, CommandSource::Builtin);
    }
}

/**
 * Verify that build takes an optional path to the project.
 */
TEST(BuiltinCommandResolverTest, BuildTakesAnOptionalPath)
{
    MockHelpRenderer helpRenderer;
    MockVersionRenderer versionRenderer;
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer, diskFileSystem());

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    const auto* build = findByName(entries, "build");
    ASSERT_NE(build, nullptr);
    ASSERT_EQ(build->spec.options.positional.size(), 1);
    EXPECT_EQ(build->spec.options.positional[0].name, "path");
    EXPECT_FALSE(build->spec.options.positional[0].required);
}

/**
 * Verify that new takes the project name as a required positional.
 */
TEST(BuiltinCommandResolverTest, NewTakesARequiredProjectName)
{
    MockHelpRenderer helpRenderer;
    MockVersionRenderer versionRenderer;
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer, diskFileSystem());

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    const auto* create = findByName(entries, "new");
    ASSERT_NE(create, nullptr);
    ASSERT_EQ(create->spec.options.positional.size(), 1);
    EXPECT_EQ(create->spec.options.positional[0].name, "project-name");
    EXPECT_TRUE(create->spec.options.positional[0].required);
}
