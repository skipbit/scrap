#include "configuration/model/ProjectConfiguration.h"
#include <catch2/catch_test_macros.hpp>

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

        auto result = config.validate();
        REQUIRE(result.has_value());
    }

    SECTION("empty name fails validation")
    {
        ProjectConfiguration config;
        config.name = "";
        config.version = "1.0.0";

        auto result = config.validate();
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("empty version fails validation")
    {
        ProjectConfiguration config;
        config.name = "test-project";
        config.version = "";

        auto result = config.validate();
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("invalid C++ standard fails validation")
    {
        ProjectConfiguration config;
        config.name = "test-project";
        config.version = "1.0.0";
        config.cppStandard = "14";

        auto result = config.validate();
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("valid C++ standards pass validation")
    {
        ProjectConfiguration config;
        config.name = "test-project";
        config.version = "1.0.0";

        config.cppStandard = "17";
        auto result17 = config.validate();
        REQUIRE(result17.has_value());

        config.cppStandard = "20";
        auto result20 = config.validate();
        REQUIRE(result20.has_value());

        config.cppStandard = "23";
        auto result23 = config.validate();
        REQUIRE(result23.has_value());
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
        auto appResult = parseProjectType("app");
        REQUIRE(appResult.has_value());
        REQUIRE(*appResult == ProjectType::Application);

        auto applicationResult = parseProjectType("application");
        REQUIRE(applicationResult.has_value());
        REQUIRE(*applicationResult == ProjectType::Application);

        auto libResult = parseProjectType("lib");
        REQUIRE(libResult.has_value());
        REQUIRE(*libResult == ProjectType::Library);

        auto libraryResult = parseProjectType("library");
        REQUIRE(libraryResult.has_value());
        REQUIRE(*libraryResult == ProjectType::Library);
    }

    SECTION("parseProjectType returns error on invalid input")
    {
        auto result = parseProjectType("invalid");
        REQUIRE_FALSE(result.has_value());
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
        auto nativeResult = parseBuildSystem("native");
        REQUIRE(nativeResult.has_value());
        REQUIRE(*nativeResult == BuildSystem::Native);

        auto scrapResult = parseBuildSystem("scrap");
        REQUIRE(scrapResult.has_value());
        REQUIRE(*scrapResult == BuildSystem::Native);

        auto cmakeResult = parseBuildSystem("cmake");
        REQUIRE(cmakeResult.has_value());
        REQUIRE(*cmakeResult == BuildSystem::CMake);

        auto mesonResult = parseBuildSystem("meson");
        REQUIRE(mesonResult.has_value());
        REQUIRE(*mesonResult == BuildSystem::Meson);

        auto bazelResult = parseBuildSystem("bazel");
        REQUIRE(bazelResult.has_value());
        REQUIRE(*bazelResult == BuildSystem::Bazel);
    }

    SECTION("parseBuildSystem returns error on invalid input")
    {
        auto result = parseBuildSystem("invalid");
        REQUIRE_FALSE(result.has_value());
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