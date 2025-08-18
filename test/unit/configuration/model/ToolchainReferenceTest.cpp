#include "configuration/model/ToolchainReference.h"
#include <catch2/catch_test_macros.hpp>

using namespace scrap::Configuration::Model;

TEST_CASE("ToolchainReference default constructor", "[configuration][model]")
{
    SECTION("creates system default toolchain")
    {
        ToolchainReference toolchain;

        REQUIRE(toolchain.isSystemDefault());
        REQUIRE(toolchain.toString() == "system");
        REQUIRE_FALSE(toolchain.version().has_value());
    }
}

TEST_CASE("ToolchainReference createSystemDefault factory", "[configuration][model]")
{
    SECTION("creates system default toolchain")
    {
        auto toolchain = ToolchainReference::createSystemDefault();

        REQUIRE(toolchain.isSystemDefault());
        REQUIRE(toolchain.toString() == "system");
        REQUIRE_FALSE(toolchain.version().has_value());
    }
}

TEST_CASE("ToolchainReference createManaged factory", "[configuration][model]")
{
    SECTION("creates managed toolchain without version")
    {
        auto result = ToolchainReference::createManaged("llvm");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        REQUIRE_FALSE(toolchain.isSystemDefault());
        auto nameResult = toolchain.name();
        REQUIRE(nameResult.has_value());
        REQUIRE(nameResult.value() == "llvm");
        REQUIRE_FALSE(toolchain.version().has_value());
        REQUIRE(toolchain.toString() == "llvm");
    }

    SECTION("creates managed toolchain with version")
    {
        auto result = ToolchainReference::createManaged("llvm", "18.0.0");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        REQUIRE_FALSE(toolchain.isSystemDefault());
        auto nameResult = toolchain.name();
        REQUIRE(nameResult.has_value());
        REQUIRE(nameResult.value() == "llvm");
        REQUIRE(toolchain.version().has_value());
        REQUIRE(toolchain.version().value() == "18.0.0");
        REQUIRE(toolchain.toString() == "llvm@18.0.0");
    }

    SECTION("returns error on empty name")
    {
        auto result = ToolchainReference::createManaged("");

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().message() == "Toolchain name cannot be empty");
    }
}

TEST_CASE("ToolchainReference parse factory", "[configuration][model]")
{
    SECTION("parses system default")
    {
        auto result1 = ToolchainReference::parse("system");
        auto result2 = ToolchainReference::parse("system-default");

        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());
        auto toolchain1 = result1.value();
        auto toolchain2 = result2.value();
        REQUIRE(toolchain1.isSystemDefault());
        REQUIRE(toolchain2.isSystemDefault());
        REQUIRE(toolchain1.toString() == "system");
        REQUIRE(toolchain2.toString() == "system");
    }

    SECTION("parses managed toolchain without version")
    {
        auto result = ToolchainReference::parse("gcc");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        REQUIRE_FALSE(toolchain.isSystemDefault());
        auto nameResult = toolchain.name();
        REQUIRE(nameResult.has_value());
        REQUIRE(nameResult.value() == "gcc");
        REQUIRE_FALSE(toolchain.version().has_value());
        REQUIRE(toolchain.toString() == "gcc");
    }

    SECTION("parses managed toolchain with version")
    {
        auto result = ToolchainReference::parse("llvm@19.0.0");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        REQUIRE_FALSE(toolchain.isSystemDefault());
        auto nameResult = toolchain.name();
        REQUIRE(nameResult.has_value());
        REQUIRE(nameResult.value() == "llvm");
        REQUIRE(toolchain.version().has_value());
        REQUIRE(toolchain.version().value() == "19.0.0");
        REQUIRE(toolchain.toString() == "llvm@19.0.0");
    }

    SECTION("handles complex version numbers")
    {
        auto result = ToolchainReference::parse("gcc@13.2.0-ubuntu");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        REQUIRE_FALSE(toolchain.isSystemDefault());
        auto nameResult = toolchain.name();
        REQUIRE(nameResult.has_value());
        REQUIRE(nameResult.value() == "gcc");
        REQUIRE(toolchain.version().value() == "13.2.0-ubuntu");
        REQUIRE(toolchain.toString() == "gcc@13.2.0-ubuntu");
    }

    SECTION("returns error on empty specification")
    {
        auto result = ToolchainReference::parse("");

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().message() == "Toolchain specification cannot be empty");
    }

    SECTION("returns error on empty version after @")
    {
        auto result = ToolchainReference::parse("llvm@");

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().message() == "Version cannot be empty after '@'");
    }
}

TEST_CASE("ToolchainReference equality comparison", "[configuration][model]")
{
    SECTION("system default toolchains are equal")
    {
        auto toolchain1 = ToolchainReference::createSystemDefault();
        auto toolchain2 = ToolchainReference();

        REQUIRE(toolchain1 == toolchain2);
        REQUIRE_FALSE(toolchain1 != toolchain2);
    }

    SECTION("managed toolchains with same name and no version are equal")
    {
        auto result1 = ToolchainReference::createManaged("llvm");
        auto result2 = ToolchainReference::parse("llvm");

        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());
        auto toolchain1 = result1.value();
        auto toolchain2 = result2.value();
        REQUIRE(toolchain1 == toolchain2);
        REQUIRE_FALSE(toolchain1 != toolchain2);
    }

    SECTION("managed toolchains with same name and version are equal")
    {
        auto result1 = ToolchainReference::createManaged("llvm", "18.0.0");
        auto result2 = ToolchainReference::parse("llvm@18.0.0");

        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());
        auto toolchain1 = result1.value();
        auto toolchain2 = result2.value();
        REQUIRE(toolchain1 == toolchain2);
        REQUIRE_FALSE(toolchain1 != toolchain2);
    }

    SECTION("different types are not equal")
    {
        auto system = ToolchainReference::createSystemDefault();
        auto result = ToolchainReference::createManaged("llvm");

        REQUIRE(result.has_value());
        auto managed = result.value();
        REQUIRE(system != managed);
        REQUIRE_FALSE(system == managed);
    }

    SECTION("different names are not equal")
    {
        auto result1 = ToolchainReference::createManaged("llvm");
        auto result2 = ToolchainReference::createManaged("gcc");

        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());
        auto llvm = result1.value();
        auto gcc = result2.value();
        REQUIRE(llvm != gcc);
        REQUIRE_FALSE(llvm == gcc);
    }

    SECTION("different versions are not equal")
    {
        auto result1 = ToolchainReference::createManaged("llvm", "18.0.0");
        auto result2 = ToolchainReference::createManaged("llvm", "19.0.0");

        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());
        auto v18 = result1.value();
        auto v19 = result2.value();
        REQUIRE(v18 != v19);
        REQUIRE_FALSE(v18 == v19);
    }

    SECTION("versioned vs unversioned are not equal")
    {
        auto result1 = ToolchainReference::createManaged("llvm", "18.0.0");
        auto result2 = ToolchainReference::createManaged("llvm");

        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());
        auto versioned = result1.value();
        auto unversioned = result2.value();
        REQUIRE(versioned != unversioned);
        REQUIRE_FALSE(versioned == unversioned);
    }
}

TEST_CASE("ToolchainReference getter methods", "[configuration][model]")
{
    SECTION("system default returns error on name access")
    {
        auto toolchain = ToolchainReference::createSystemDefault();

        auto nameResult = toolchain.name();
        REQUIRE_FALSE(nameResult.has_value());
        REQUIRE(nameResult.error().message() == "System default toolchain has no name");
    }

    SECTION("managed toolchain provides name")
    {
        auto result = ToolchainReference::createManaged("clang");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        auto nameResult = toolchain.name();
        REQUIRE(nameResult.has_value());
        REQUIRE(nameResult.value() == "clang");
    }

    SECTION("version returns nullopt for system default")
    {
        auto toolchain = ToolchainReference::createSystemDefault();

        REQUIRE_FALSE(toolchain.version().has_value());
    }

    SECTION("version returns nullopt for unversioned managed")
    {
        auto result = ToolchainReference::createManaged("gcc");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        REQUIRE_FALSE(toolchain.version().has_value());
    }

    SECTION("version returns value for versioned managed")
    {
        auto result = ToolchainReference::createManaged("gcc", "12.3.0");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        REQUIRE(toolchain.version().has_value());
        REQUIRE(toolchain.version().value() == "12.3.0");
    }
}

TEST_CASE("ToolchainReference toString method", "[configuration][model]")
{
    SECTION("system default returns 'system'")
    {
        auto toolchain = ToolchainReference::createSystemDefault();

        REQUIRE(toolchain.toString() == "system");
    }

    SECTION("unversioned managed returns name only")
    {
        auto result = ToolchainReference::createManaged("gcc");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        REQUIRE(toolchain.toString() == "gcc");
    }

    SECTION("versioned managed returns name@version")
    {
        auto result = ToolchainReference::createManaged("llvm", "17.0.6");

        REQUIRE(result.has_value());
        auto toolchain = result.value();
        REQUIRE(toolchain.toString() == "llvm@17.0.6");
    }
}

TEST_CASE("ToolchainReference round-trip consistency", "[configuration][model]")
{
    SECTION("system default")
    {
        auto original = ToolchainReference::createSystemDefault();
        auto parseResult = ToolchainReference::parse(original.toString());

        REQUIRE(parseResult.has_value());
        auto roundTrip = parseResult.value();
        REQUIRE(original == roundTrip);
    }

    SECTION("unversioned managed")
    {
        auto result = ToolchainReference::createManaged("clang");
        REQUIRE(result.has_value());
        auto original = result.value();

        auto parseResult = ToolchainReference::parse(original.toString());
        REQUIRE(parseResult.has_value());
        auto roundTrip = parseResult.value();

        REQUIRE(original == roundTrip);
    }

    SECTION("versioned managed")
    {
        auto result = ToolchainReference::createManaged("gcc", "11.4.0");
        REQUIRE(result.has_value());
        auto original = result.value();

        auto parseResult = ToolchainReference::parse(original.toString());
        REQUIRE(parseResult.has_value());
        auto roundTrip = parseResult.value();

        REQUIRE(original == roundTrip);
    }
}
