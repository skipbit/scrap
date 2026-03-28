#include <gtest/gtest.h>

#include "command/BuiltinCommandResolver.h"
#include "command/CommandCatalog.h"
#include "command/ExternalCommandResolver.h"
#include "command/ProjectCommandResolver.h"
#include "command/StubScriptsReader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

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
 * Mock ExternalMetadataProvider that derives metadata from the filename.
 */
class MockMetadataProvider : public ExternalMetadataProvider {
public:
    /**
     * Return metadata with name derived from executable filename.
     */
    auto fetch(const std::filesystem::path& executable) -> std::expected<ExternalCommandMetadata, std::string> override
    {
        ExternalCommandMetadata meta;
        meta.name = executable.filename().string().substr(6);  // strip "scrap-"
        meta.description = "Mock description for " + meta.name;
        return meta;
    }
};

/**
 * Mock ScriptsReader that returns a configurable result.
 */
class MockScriptsReader : public ScriptsReader {
public:
    /**
     * Construct with the result to return from read().
     */
    explicit MockScriptsReader(std::expected<std::vector<ScriptDef>, std::string> result)
        : result_(std::move(result))
    {
    }

    /**
     * Return the pre-configured result.
     */
    auto read([[maybe_unused]] const std::filesystem::path& projectRoot)
        -> std::expected<std::vector<ScriptDef>, std::string> override
    {
        return result_;
    }

private:
    std::expected<std::vector<ScriptDef>, std::string> result_;
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

/**
 * Test fixture providing temp directory for external command tests.
 */
class ExternalCommandResolverTest : public ::testing::Test {
protected:
    /**
     * Create a temp directory for test fixtures.
     */
    void SetUp() override
    {
        tempDir_ = std::filesystem::temp_directory_path() / "scrap_resolver_test";
        std::filesystem::create_directories(tempDir_);
    }

    /**
     * Clean up the temp directory.
     */
    void TearDown() override
    {
        std::filesystem::remove_all(tempDir_);
    }

    /**
     * Create a shell script with execute permission.
     */
    void createExecutable(const std::string& name)
    {
        auto path = tempDir_ / name;
        std::ofstream(path) << "#!/bin/sh\necho hello\n";
        std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    }

    /**
     * Create a regular file without execute permission.
     */
    void createNonExecutable(const std::string& name)
    {
        auto path = tempDir_ / name;
        std::ofstream(path) << "not executable";
    }

    std::filesystem::path tempDir_;
};

/**
 * Verify that scrap-* executables are discovered from searchPaths.
 */
TEST_F(ExternalCommandResolverTest, FindsScrapPrefixedExecutables)
{
    createExecutable("scrap-lint");
    createExecutable("scrap-fmt");

    auto provider = std::make_unique<MockMetadataProvider>();
    ExternalCommandResolver resolver(std::move(provider));

    RuntimeEnvironment env;
    env.searchPaths = {tempDir_};
    auto entries = resolver.resolve(env);

    EXPECT_EQ(entries.size(), 2);
    EXPECT_NE(findByName(entries, "lint"), nullptr);
    EXPECT_NE(findByName(entries, "fmt"), nullptr);
}

/**
 * Verify that files not starting with scrap- are ignored.
 */
TEST_F(ExternalCommandResolverTest, SkipsNonScrapFiles)
{
    createExecutable("scrap-lint");
    createExecutable("other-tool");

    auto provider = std::make_unique<MockMetadataProvider>();
    ExternalCommandResolver resolver(std::move(provider));

    RuntimeEnvironment env;
    env.searchPaths = {tempDir_};
    auto entries = resolver.resolve(env);

    EXPECT_EQ(entries.size(), 1);
    EXPECT_NE(findByName(entries, "lint"), nullptr);
}

/**
 * Verify that non-executable scrap-* files are ignored.
 */
TEST_F(ExternalCommandResolverTest, SkipsNonExecutables)
{
    createNonExecutable("scrap-lint");

    auto provider = std::make_unique<MockMetadataProvider>();
    ExternalCommandResolver resolver(std::move(provider));

    RuntimeEnvironment env;
    env.searchPaths = {tempDir_};
    auto entries = resolver.resolve(env);

    EXPECT_TRUE(entries.empty());
}

/**
 * Verify that empty searchPaths produces no entries.
 */
TEST_F(ExternalCommandResolverTest, EmptySearchPathsReturnsEmpty)
{
    auto provider = std::make_unique<MockMetadataProvider>();
    ExternalCommandResolver resolver(std::move(provider));

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    EXPECT_TRUE(entries.empty());
}

/**
 * Verify that discovered entries have External source.
 */
TEST_F(ExternalCommandResolverTest, EntriesHaveExternalSource)
{
    createExecutable("scrap-lint");

    auto provider = std::make_unique<MockMetadataProvider>();
    ExternalCommandResolver resolver(std::move(provider));

    RuntimeEnvironment env;
    env.searchPaths = {tempDir_};
    auto entries = resolver.resolve(env);

    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].source, CommandSource::External);
}

/**
 * Verify that StubScriptsReader returns an empty entry list.
 */
TEST(ProjectCommandResolverTest, StubReaderReturnsEmpty)
{
    auto reader = std::make_unique<StubScriptsReader>();
    ProjectCommandResolver resolver(std::move(reader));

    RuntimeEnvironment env;
    env.projectRoot = "/tmp/nonexistent";
    auto entries = resolver.resolve(env);

    EXPECT_TRUE(entries.empty());
}

/**
 * Verify that read errors result in an empty entry list.
 */
TEST(ProjectCommandResolverTest, ErrorReaderReturnsEmpty)
{
    auto reader = std::make_unique<MockScriptsReader>(std::unexpected(std::string{"file not found"}));
    ProjectCommandResolver resolver(std::move(reader));

    RuntimeEnvironment env;
    env.projectRoot = "/tmp/nonexistent";
    auto entries = resolver.resolve(env);

    EXPECT_TRUE(entries.empty());
}

/**
 * Verify that ScriptDefs are correctly converted to CommandEntry list.
 */
TEST(ProjectCommandResolverTest, ScriptDefsConvertToEntries)
{
    std::vector<ScriptDef> scripts = {
        {.name = "deploy", .description = "Deploy the project", .command = "make deploy"},
        {.name = "lint", .description = "Run linter", .command = "make lint"},
    };
    auto reader = std::make_unique<MockScriptsReader>(std::move(scripts));
    ProjectCommandResolver resolver(std::move(reader));

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    ASSERT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].spec.name, "deploy");
    EXPECT_EQ(entries[0].spec.description, "Deploy the project");
    EXPECT_EQ(entries[0].source, CommandSource::Project);
    EXPECT_EQ(entries[1].spec.name, "lint");
}
