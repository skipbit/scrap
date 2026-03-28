#include <gtest/gtest.h>

#include "command/ProjectCommandResolver.h"
#include "command/ScriptsReader.h"
#include "command/StubScriptsReader.h"

using namespace scrap::Command;

namespace {

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

}  // namespace

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
