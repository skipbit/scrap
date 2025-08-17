#include "configuration/model/Configuration.h"
#include "configuration/model/ConfigurationSource.h"
#include "configuration/model/ToolchainReference.h"
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace scrap::configuration::model;

TEST_CASE("Configuration loading and validation", "[configuration][model]")
{

    SECTION("creates configuration with defaults")
    {
        Configuration config;
        config.applyDefaults();

        REQUIRE(config.isComplete());
        REQUIRE(config.toolchain().value().isSystemDefault());
    }

    SECTION("configuration with managed toolchain")
    {
        Configuration config;
        auto toolchain = ToolchainReference::createManaged("llvm", "18.0.0");

        config.setToolchain(toolchain, ConfigurationSource::Environment);

        REQUIRE(config.isComplete());
        REQUIRE_FALSE(config.toolchain().value().isSystemDefault());
        REQUIRE(config.toolchain().value().name() == "llvm");
        REQUIRE(config.toolchain().value().version() == "18.0.0");
        REQUIRE(config.toolchain().source() == ConfigurationSource::Environment);
    }

    SECTION("configuration precedence rules")
    {
        Configuration config;

        // Set from environment first
        auto envToolchain = ToolchainReference::createManaged("gcc", "13.0.0");
        config.setToolchain(envToolchain, ConfigurationSource::Environment);

        // Try to override with lower precedence (should not change)
        auto repoToolchain = ToolchainReference::createManaged("llvm", "18.0.0");
        config.setToolchain(repoToolchain, ConfigurationSource::RepositoryMarker);

        REQUIRE(config.toolchain().value().name() == "gcc");
        REQUIRE(config.toolchain().source() == ConfigurationSource::Environment);
    }
}

TEST_CASE("ToolchainReference model operations", "[configuration][model]")
{

    SECTION("system default toolchain")
    {
        auto toolchain = ToolchainReference::createSystemDefault();

        REQUIRE(toolchain.isSystemDefault());
        REQUIRE(toolchain.toString() == "system");
    }

    SECTION("managed toolchain without version")
    {
        auto toolchain = ToolchainReference::createManaged("gcc");

        REQUIRE_FALSE(toolchain.isSystemDefault());
        REQUIRE(toolchain.name() == "gcc");
        REQUIRE_FALSE(toolchain.version().has_value());
        REQUIRE(toolchain.toString() == "gcc");
    }

    SECTION("managed toolchain with version")
    {
        auto toolchain = ToolchainReference::createManaged("llvm", "18.0.0");

        REQUIRE_FALSE(toolchain.isSystemDefault());
        REQUIRE(toolchain.name() == "llvm");
        REQUIRE(toolchain.version() == "18.0.0");
        REQUIRE(toolchain.toString() == "llvm@18.0.0");
    }

    SECTION("parsing toolchain specifications")
    {
        auto system = ToolchainReference::parse("system");
        REQUIRE(system.isSystemDefault());

        auto gcc = ToolchainReference::parse("gcc");
        REQUIRE(gcc.name() == "gcc");
        REQUIRE_FALSE(gcc.version().has_value());

        auto llvm = ToolchainReference::parse("llvm@18.0.0");
        REQUIRE(llvm.name() == "llvm");
        REQUIRE(llvm.version() == "18.0.0");
    }

    SECTION("toolchain equality")
    {
        auto toolchain1 = ToolchainReference::createManaged("llvm", "18.0.0");
        auto toolchain2 = ToolchainReference::createManaged("llvm", "18.0.0");
        auto toolchain3 = ToolchainReference::createManaged("gcc", "13.0.0");

        REQUIRE(toolchain1 == toolchain2);
        REQUIRE(toolchain1 != toolchain3);
    }
}
