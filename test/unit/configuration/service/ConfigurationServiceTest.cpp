#include "configuration/service/ConfigurationService.h"
#include "configuration/model/Configuration.h"
#include "configuration/model/ProjectConfiguration.h"
#include "configuration/model/ToolchainReference.h"
#include "helpers/FileSystemHelper.h"
#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace scrap::configuration::service;
using namespace scrap::Configuration::Model;
using namespace scrap::test;

// Mock implementation for testing
class MockConfigurationService : public ConfigurationService {
public:
    Configuration loadConfiguration([[maybe_unused]] const std::filesystem::path& workingDirectory,
                                    const std::optional<ToolchainReference>& cliToolchain) override
    {

        Configuration config;

        // Simulate CLI toolchain override
        if (cliToolchain.has_value()) {
            config.setToolchain(cliToolchain.value(), ConfigurationSource::CommandLine);
        } else {
            config.applyDefaults();
        }

        return config;
    }

    std::optional<ProjectConfiguration> loadProjectConfiguration(const std::filesystem::path& projectPath) override
    {

        auto scrapToml = projectPath / "scrap.toml";
        if (!std::filesystem::exists(scrapToml)) {
            return std::nullopt;
        }

        auto config = ProjectConfiguration::createDefault("test-project", ProjectType::Application);
        config.version = "1.0.0";
        config.cppStandard = "23";

        return config;
    }

    void saveProjectConfiguration(const std::filesystem::path& projectPath, const ProjectConfiguration& config) override
    {

        auto scrapToml = projectPath / "scrap.toml";

        // Simulate TOML generation
        const std::string tomlContent = "[package]\n"
                                        "name = \"" +
            config.name +
            "\"\n"
            "version = \"" +
            config.version +
            "\"\n"
            "type = \"" +
            toString(config.type) +
            "\"\n"
            "std = \"" +
            config.cppStandard + "\"\n";

        FileSystemHelper::createFile(scrapToml, tomlContent);
    }

    void createDefaultConfiguration(const std::filesystem::path& projectPath,
                                    const std::string& projectName,
                                    ProjectType projectType,
                                    const std::optional<ToolchainReference>& toolchain) override
    {

        auto config = ProjectConfiguration::createDefault(projectName, projectType);
        if (toolchain.has_value()) {
            config.toolchain = toolchain.value();
        }
        saveProjectConfiguration(projectPath, config);
    }

    void setProjectToolchain([[maybe_unused]] const std::filesystem::path& projectPath,
                             [[maybe_unused]] const ToolchainReference& toolchain) override
    {
        // Mock implementation - would modify scrap.toml
    }

    void setRepositoryToolchain([[maybe_unused]] const std::filesystem::path& repositoryRoot,
                                [[maybe_unused]] const ToolchainReference& toolchain) override
    {
        // Mock implementation - would create .scrap-toolchain file
    }

    std::vector<std::string> validateConfiguration(const Configuration& config) override
    {
        std::vector<std::string> errors;
        if (!config.isComplete()) {
            errors.emplace_back("Configuration is incomplete");
        }
        return errors;
    }
};

TEST_CASE("ConfigurationService operations", "[configuration][service]")
{
    auto service = std::make_unique<MockConfigurationService>();

    SECTION("load configuration with CLI toolchain override")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("config_test_");
        auto cliToolchain = ToolchainReference::createManaged("clang", "17.0.0");

        auto config = service->loadConfiguration(tempDir.path(), cliToolchain);

        REQUIRE(config.isComplete());
        REQUIRE(config.toolchain().value().name() == "clang");
        REQUIRE(config.toolchain().value().version() == "17.0.0");
    }

    SECTION("load configuration without CLI override")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("config_test_");

        auto config = service->loadConfiguration(tempDir.path(), std::nullopt);

        REQUIRE(config.isComplete());
        REQUIRE(config.toolchain().value().isSystemDefault());
    }

    SECTION("load project configuration")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("config_test_");

        SECTION("existing scrap.toml")
        {
            FileSystemHelper::createFile(tempDir.path() / "scrap.toml", "[package]\nname = \"test\"\n");

            auto result = service->loadProjectConfiguration(tempDir.path());

            REQUIRE(result.has_value());
            if (result.has_value()) {
                REQUIRE(result->name == "test-project");
                REQUIRE(result->version == "1.0.0");
                REQUIRE(result->type == ProjectType::Application);
                REQUIRE(result->cppStandard == "23");
            }
        }

        SECTION("no scrap.toml")
        {
            auto result = service->loadProjectConfiguration(tempDir.path());

            REQUIRE_FALSE(result.has_value());
        }
    }

    SECTION("create default configuration")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("config_test_");

        service->createDefaultConfiguration(tempDir.path(), "my-project", ProjectType::Library, std::nullopt);

        auto scrapToml = tempDir.path() / "scrap.toml";
        REQUIRE(FileSystemHelper::exists(scrapToml));

        auto content = FileSystemHelper::readFile(scrapToml);
        REQUIRE(content.find("name = \"my-project\"") != std::string::npos);
        REQUIRE(content.find("type = \"lib\"") != std::string::npos);
    }

    SECTION("validate configuration")
    {
        Configuration config;

        auto errors = service->validateConfiguration(config);
        REQUIRE(errors.size() == 1);
        REQUIRE(errors[0] == "Configuration is incomplete");

        config.applyDefaults();
        errors = service->validateConfiguration(config);
        REQUIRE(errors.empty());
    }
}
