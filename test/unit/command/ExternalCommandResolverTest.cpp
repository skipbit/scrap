#include <gtest/gtest.h>

#include "command/ExternalCommandResolver.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

using namespace scrap::Command;

namespace {

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
 * Test fixture providing a temp directory for external command tests.
 */
class ExternalCommandResolverTest : public ::testing::Test {
protected:
    /**
     * Create a per-test temp directory.
     *
     * Each test case runs as its own ctest entry and ctest may run them
     * in parallel, so the directory name must be unique per test case.
     */
    void SetUp() override
    {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tempDir_ = std::filesystem::temp_directory_path() / (std::string("scrap_resolver_test_") + info->name());
        std::filesystem::remove_all(tempDir_);
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
