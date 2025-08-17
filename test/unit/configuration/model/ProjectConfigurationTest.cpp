#include "configuration/model/ProjectConfiguration.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <stdexcept>

using namespace scrap::Configuration::Model;

TEST_CASE("ProjectConfiguration creation and defaults", "[configuration][model]")
{
    SECTION("default constructor creates valid configuration")
    {
        ProjectConfiguration config;

        REQUIRE(config.name.empty());
        REQUIRE(config.version == "0.1.0");
        REQUIRE(config.type == ProjectType::Application);
        REQUIRE(config.cppStandard == "23");
        REQUIRE(config.buildSystem == BuildSystem::Native);
        REQUIRE_FALSE(config.toolchain.has_value());
        REQUIRE(config.testFramework == "scrap");
        REQUIRE(config.testPatterns.size() == 2);
        REQUIRE(config.testPatterns[0] == "*_test.cpp");
        REQUIRE(config.testPatterns[1] == "test_*.cpp");
    }

    SECTION("createDefault factory method")
    {
        auto config = ProjectConfiguration::createDefault("myapp", ProjectType::Application);

        REQUIRE(config.name == "myapp");
        REQUIRE(config.type == ProjectType::Application);
        REQUIRE(config.version == "0.1.0");
        REQUIRE(config.cppStandard == "23");
    }

    SECTION("createDefault for library project")
    {
        auto config = ProjectConfiguration::createDefault("mylib", ProjectType::Library);

        REQUIRE(config.name == "mylib");
        REQUIRE(config.type == ProjectType::Library);
        REQUIRE(config.isLibrary());
        REQUIRE_FALSE(config.isApplication());
    }
}

TEST_CASE("ProjectConfiguration type checking methods", "[configuration][model]")
{
    SECTION("application project type checks")
    {
        ProjectConfiguration config;
        config.type = ProjectType::Application;

        REQUIRE(config.isApplication());
        REQUIRE_FALSE(config.isLibrary());
    }

    SECTION("library project type checks")
    {
        ProjectConfiguration config;
        config.type = ProjectType::Library;

        REQUIRE(config.isLibrary());
        REQUIRE_FALSE(config.isApplication());
    }
}

TEST_CASE("ProjectConfiguration build system checks", "[configuration][model]")
{
    SECTION("native build system")
    {
        ProjectConfiguration config;
        config.buildSystem = BuildSystem::Native;

        REQUIRE(config.isNativeBuild());
        REQUIRE_FALSE(config.isWrapperMode());
    }

    SECTION("cmake wrapper mode")
    {
        ProjectConfiguration config;
        config.buildSystem = BuildSystem::CMake;

        REQUIRE_FALSE(config.isNativeBuild());
        REQUIRE(config.isWrapperMode());
    }

    SECTION("meson wrapper mode")
    {
        ProjectConfiguration config;
        config.buildSystem = BuildSystem::Meson;

        REQUIRE_FALSE(config.isNativeBuild());
        REQUIRE(config.isWrapperMode());
    }

    SECTION("bazel wrapper mode")
    {
        ProjectConfiguration config;
        config.buildSystem = BuildSystem::Bazel;

        REQUIRE_FALSE(config.isNativeBuild());
        REQUIRE(config.isWrapperMode());
    }
}

TEST_CASE("ProjectConfiguration validation", "[configuration][model]")
{
    SECTION("valid configuration passes validation")
    {
        ProjectConfiguration config;
        config.name = "valid-project";
        config.version = "1.0.0";
        config.cppStandard = "20";

        REQUIRE_NOTHROW(config.validate());
    }

    SECTION("empty name fails validation")
    {
        ProjectConfiguration config;
        config.name = "";
        config.version = "1.0.0";

        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);
    }

    SECTION("empty version fails validation")
    {
        ProjectConfiguration config;
        config.name = "test-project";
        config.version = "";

        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);
    }

    SECTION("invalid C++ standard fails validation")
    {
        ProjectConfiguration config;
        config.name = "test-project";
        config.version = "1.0.0";
        config.cppStandard = "14";

        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);
    }

    SECTION("valid C++ standards pass validation")
    {
        ProjectConfiguration config;
        config.name = "test-project";
        config.version = "1.0.0";

        config.cppStandard = "17";
        REQUIRE_NOTHROW(config.validate());

        config.cppStandard = "20";
        REQUIRE_NOTHROW(config.validate());

        config.cppStandard = "23";
        REQUIRE_NOTHROW(config.validate());
    }
}

TEST_CASE("ProjectType enum utilities", "[configuration][model]")
{
    SECTION("toString converts ProjectType to string")
    {
        REQUIRE(toString(ProjectType::Application) == "app");
        REQUIRE(toString(ProjectType::Library) == "lib");
    }

    SECTION("parseProjectType converts string to ProjectType")
    {
        REQUIRE(parseProjectType("app") == ProjectType::Application);
        REQUIRE(parseProjectType("application") == ProjectType::Application);
        REQUIRE(parseProjectType("lib") == ProjectType::Library);
        REQUIRE(parseProjectType("library") == ProjectType::Library);
    }

    SECTION("parseProjectType throws on invalid input")
    {
        REQUIRE_THROWS_AS(parseProjectType("invalid"), std::invalid_argument);
    }
}

TEST_CASE("BuildSystem enum utilities", "[configuration][model]")
{
    SECTION("toString converts BuildSystem to string")
    {
        REQUIRE(toString(BuildSystem::Native) == "native");
        REQUIRE(toString(BuildSystem::CMake) == "cmake");
        REQUIRE(toString(BuildSystem::Meson) == "meson");
        REQUIRE(toString(BuildSystem::Bazel) == "bazel");
    }

    SECTION("parseBuildSystem converts string to BuildSystem")
    {
        REQUIRE(parseBuildSystem("native") == BuildSystem::Native);
        REQUIRE(parseBuildSystem("scrap") == BuildSystem::Native);
        REQUIRE(parseBuildSystem("cmake") == BuildSystem::CMake);
        REQUIRE(parseBuildSystem("meson") == BuildSystem::Meson);
        REQUIRE(parseBuildSystem("bazel") == BuildSystem::Bazel);
    }

    SECTION("parseBuildSystem throws on invalid input")
    {
        REQUIRE_THROWS_AS(parseBuildSystem("invalid"), std::invalid_argument);
    }
}

TEST_CASE("ProjectConfiguration containers and collections", "[configuration][model]")
{
    SECTION("containers are initialized empty")
    {
        const ProjectConfiguration config;

        REQUIRE(config.cxxFlags.empty());
        REQUIRE(config.linkFlags.empty());
        REQUIRE(config.buildOptions.empty());
        REQUIRE(config.dependencies.empty());
        REQUIRE(config.devDependencies.empty());
        REQUIRE(config.toolOptions.empty());
    }

    SECTION("containers can be populated")
    {
        ProjectConfiguration config;

        config.cxxFlags.emplace_back("-Wall");
        config.cxxFlags.emplace_back("-Wextra");
        REQUIRE(config.cxxFlags.size() == 2);

        config.dependencies["fmt"] = "10.2.1";
        config.dependencies["spdlog"] = ">=1.12";
        REQUIRE(config.dependencies.size() == 2);
        REQUIRE(config.dependencies["fmt"] == "10.2.1");

        config.buildOptions["optimization"] = "O3";
        REQUIRE(config.buildOptions["optimization"] == "O3");
    }
}