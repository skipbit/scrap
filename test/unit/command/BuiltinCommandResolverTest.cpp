#include <gtest/gtest.h>

#include "command/BuiltinCommandResolver.h"
#include "command/CommandCatalog.h"

#include <algorithm>

using namespace scrap::Command;

namespace {

/**
 * Minimal HelpRenderer mock for BuiltinCommandResolver DI.
 */
class MockHelpRenderer : public HelpRenderer {
public:
    /**
     * Return a fixed global help string.
     */
    auto renderGlobal([[maybe_unused]] std::span<const HelpEntry> entries) const -> std::string override
    {
        return "global help";
    }

    /**
     * Return a fixed command help string.
     */
    auto renderCommand([[maybe_unused]] const CommandSpec& spec) const -> std::string override
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
    auto render() const -> std::string override
    {
        return "scrap 0.0.1-test";
    }
};

/**
 * Find an entry by name in a flat vector.
 */
auto findByName(const std::vector<CommandEntry>& entries, const std::string& name) -> const CommandEntry*
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
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer);

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
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer);

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
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer);

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
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer);

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
    BuiltinCommandResolver resolver(helpRenderer, versionRenderer);

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    for (const auto& entry : entries) {
        EXPECT_EQ(entry.source, CommandSource::Builtin);
    }
}
