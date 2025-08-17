#include "configuration/model/ConfigurationSource.h"
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace scrap::Configuration::Model;

TEST_CASE("ConfigurationSource enum values", "[configuration][model]")
{
    SECTION("enum values are correctly defined")
    {
        // Test that all enum values exist
        const auto cmdLine = ConfigurationSource::CommandLine;
        const auto projectConfig = ConfigurationSource::ProjectConfig;
        const auto repoMarker = ConfigurationSource::RepositoryMarker;
        const auto environment = ConfigurationSource::Environment;
        const auto systemDefault = ConfigurationSource::SystemDefault;

        // Basic existence check
        REQUIRE(static_cast<int>(cmdLine) == 0);
        REQUIRE(static_cast<int>(projectConfig) == 1);
        REQUIRE(static_cast<int>(repoMarker) == 2);
        REQUIRE(static_cast<int>(environment) == 3);
        REQUIRE(static_cast<int>(systemDefault) == 4);
    }
}

TEST_CASE("ConfigurationSource toString function", "[configuration][model]")
{
    SECTION("converts command line source")
    {
        const auto result = toString(ConfigurationSource::CommandLine);
        REQUIRE(result == "command-line");
    }

    SECTION("converts project config source")
    {
        const auto result = toString(ConfigurationSource::ProjectConfig);
        REQUIRE(result == "project configuration");
    }

    SECTION("converts repository marker source")
    {
        const auto result = toString(ConfigurationSource::RepositoryMarker);
        REQUIRE(result == "repository marker");
    }

    SECTION("converts environment source")
    {
        const auto result = toString(ConfigurationSource::Environment);
        REQUIRE(result == "environment variable");
    }

    SECTION("converts system default source")
    {
        const auto result = toString(ConfigurationSource::SystemDefault);
        REQUIRE(result == "system default");
    }
}

TEST_CASE("ConfigurationSource precedence comparison", "[configuration][model]")
{
    SECTION("command line has highest precedence")
    {
        REQUIRE(hasHigherPrecedence(ConfigurationSource::CommandLine, ConfigurationSource::ProjectConfig));
        REQUIRE(hasHigherPrecedence(ConfigurationSource::CommandLine, ConfigurationSource::RepositoryMarker));
        REQUIRE(hasHigherPrecedence(ConfigurationSource::CommandLine, ConfigurationSource::Environment));
        REQUIRE(hasHigherPrecedence(ConfigurationSource::CommandLine, ConfigurationSource::SystemDefault));
    }

    SECTION("project config has higher precedence than repo marker")
    {
        REQUIRE(hasHigherPrecedence(ConfigurationSource::ProjectConfig, ConfigurationSource::RepositoryMarker));
        REQUIRE(hasHigherPrecedence(ConfigurationSource::ProjectConfig, ConfigurationSource::Environment));
        REQUIRE(hasHigherPrecedence(ConfigurationSource::ProjectConfig, ConfigurationSource::SystemDefault));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::ProjectConfig, ConfigurationSource::CommandLine));
    }

    SECTION("repository marker has higher precedence than environment")
    {
        REQUIRE(hasHigherPrecedence(ConfigurationSource::RepositoryMarker, ConfigurationSource::Environment));
        REQUIRE(hasHigherPrecedence(ConfigurationSource::RepositoryMarker, ConfigurationSource::SystemDefault));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::RepositoryMarker, ConfigurationSource::CommandLine));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::RepositoryMarker, ConfigurationSource::ProjectConfig));
    }

    SECTION("environment has higher precedence than system default")
    {
        REQUIRE(hasHigherPrecedence(ConfigurationSource::Environment, ConfigurationSource::SystemDefault));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::Environment, ConfigurationSource::CommandLine));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::Environment, ConfigurationSource::ProjectConfig));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::Environment, ConfigurationSource::RepositoryMarker));
    }

    SECTION("system default has lowest precedence")
    {
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::SystemDefault, ConfigurationSource::CommandLine));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::SystemDefault, ConfigurationSource::ProjectConfig));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::SystemDefault, ConfigurationSource::RepositoryMarker));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::SystemDefault, ConfigurationSource::Environment));
    }

    SECTION("same source has equal precedence")
    {
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::CommandLine, ConfigurationSource::CommandLine));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::ProjectConfig, ConfigurationSource::ProjectConfig));
        REQUIRE_FALSE(hasHigherPrecedence(ConfigurationSource::Environment, ConfigurationSource::Environment));
    }
}

TEST_CASE("ConfigurationSource comprehensive precedence ordering", "[configuration][model]")
{
    SECTION("verify complete precedence chain")
    {
        // CommandLine (0) > ProjectConfig (1) > RepositoryMarker (2) > Environment (3) > SystemDefault (4)
        const std::vector<ConfigurationSource> sources = {ConfigurationSource::CommandLine,
                                                          ConfigurationSource::ProjectConfig,
                                                          ConfigurationSource::RepositoryMarker,
                                                          ConfigurationSource::Environment,
                                                          ConfigurationSource::SystemDefault};

        // Test that each source has higher precedence than all sources after it
        for (size_t i = 0; i < sources.size(); ++i) {
            for (size_t j = i + 1; j < sources.size(); ++j) {
                REQUIRE(hasHigherPrecedence(sources[i], sources[j]));
                REQUIRE_FALSE(hasHigherPrecedence(sources[j], sources[i]));
            }
        }
    }
}