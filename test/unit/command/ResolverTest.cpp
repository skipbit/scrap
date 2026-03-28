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
 * @brief Minimal HelpRenderer mock for BuiltinCommandResolver DI.
 */
class MockHelpRenderer : public HelpRenderer {
public:
    auto renderGlobal([[maybe_unused]] std::span<const HelpEntry> entries) const -> std::string override
    {
        return "global help";
    }

    auto renderCommand([[maybe_unused]] const CommandSpec& spec) const -> std::string override
    {
        return "command help";
    }
};

/**
 * @brief Minimal VersionRenderer mock for BuiltinCommandResolver DI.
 */
class MockVersionRenderer : public VersionRenderer {
public:
    auto render() const -> std::string override
    {
        return "scrap 0.0.1-test";
    }
};

/**
 * @brief Mock ExternalMetadataProvider for testing.
 */
class MockMetadataProvider : public ExternalMetadataProvider {
public:
    auto fetch(const std::filesystem::path& executable) -> std::expected<ExternalCommandMetadata, std::string> override
    {
        ExternalCommandMetadata meta;
        meta.name = executable.filename().string().substr(6);  // strip "scrap-"
        meta.description = "Mock description for " + meta.name;
        return meta;
    }
};

/**
 * @brief Mock ScriptsReader that returns a configurable result.
 */
class MockScriptsReader : public ScriptsReader {
public:
    explicit MockScriptsReader(std::expected<std::vector<ScriptDef>, std::string> result)
        : result_(std::move(result))
    {
    }

    auto read([[maybe_unused]] const std::filesystem::path& projectRoot)
        -> std::expected<std::vector<ScriptDef>, std::string> override
    {
        return result_;
    }

private:
    std::expected<std::vector<ScriptDef>, std::string> result_;
};

/**
 * @brief Find an entry by name in a flat vector.
 */
auto findByName(const std::vector<CommandEntry>& entries, const std::string& name) -> const CommandEntry*
{
    auto it = std::ranges::find_if(entries, [&](const CommandEntry& e) {
        return e.spec.name == name;
    });
    return it != entries.end() ? &(*it) : nullptr;
}

}  // namespace

// =============================================================================
// BuiltinCommandResolver
// =============================================================================

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

// =============================================================================
// ExternalCommandResolver
// =============================================================================

class ExternalCommandResolverTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        tempDir_ = std::filesystem::temp_directory_path() / "scrap_resolver_test";
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(tempDir_);
    }

    void createExecutable(const std::string& name)
    {
        auto path = tempDir_ / name;
        std::ofstream(path) << "#!/bin/sh\necho hello\n";
        std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    }

    void createNonExecutable(const std::string& name)
    {
        auto path = tempDir_ / name;
        std::ofstream(path) << "not executable";
    }

    std::filesystem::path tempDir_;
};

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

TEST_F(ExternalCommandResolverTest, EmptySearchPathsReturnsEmpty)
{
    auto provider = std::make_unique<MockMetadataProvider>();
    ExternalCommandResolver resolver(std::move(provider));

    RuntimeEnvironment env;
    auto entries = resolver.resolve(env);

    EXPECT_TRUE(entries.empty());
}

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

// =============================================================================
// ProjectCommandResolver
// =============================================================================

TEST(ProjectCommandResolverTest, StubReaderReturnsEmpty)
{
    auto reader = std::make_unique<StubScriptsReader>();
    ProjectCommandResolver resolver(std::move(reader));

    RuntimeEnvironment env;
    env.projectRoot = "/tmp/nonexistent";
    auto entries = resolver.resolve(env);

    EXPECT_TRUE(entries.empty());
}

TEST(ProjectCommandResolverTest, ErrorReaderReturnsEmpty)
{
    auto reader = std::make_unique<MockScriptsReader>(std::unexpected(std::string{"file not found"}));
    ProjectCommandResolver resolver(std::move(reader));

    RuntimeEnvironment env;
    env.projectRoot = "/tmp/nonexistent";
    auto entries = resolver.resolve(env);

    EXPECT_TRUE(entries.empty());
}

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
