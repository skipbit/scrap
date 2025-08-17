#include "template/service/TemplateProcessor.h"
#include "template/model/Template.h"
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace scrap::template_system::service;
using namespace scrap::template_system::model;

TEST_CASE("TemplateProcessor variable substitution", "[template][processor]")
{
    TemplateProcessor processor;

    SECTION("replaces simple variables")
    {
        const std::string input = "Hello {{name}}!";
        VariableMap vars;
        vars.set("name", "World");

        auto result = processor.processContent(input, vars);

        REQUIRE(result == "Hello World!");
    }

    SECTION("handles multiple variables")
    {
        const std::string input = "{{greeting}} {{name}}!";
        VariableMap vars;
        vars.set("greeting", "Hello");
        vars.set("name", "scrap");

        auto result = processor.processContent(input, vars);

        REQUIRE(result == "Hello scrap!");
    }

    SECTION("preserves text without variables")
    {
        const std::string input = "No variables here!";
        const VariableMap vars;

        auto result = processor.processContent(input, vars);

        REQUIRE(result == input);
    }

    SECTION("handles missing variables by leaving placeholder")
    {
        const std::string input = "Hello {{missing}}!";
        const VariableMap vars;

        auto result = processor.processContent(input, vars);

        REQUIRE(result == "Hello {{missing}}!");
    }

    SECTION("handles variables with transforms")
    {
        const std::string input = "Class: {{name|PascalCase}}, file: {{name|snake_case}}.cpp";
        VariableMap vars;
        vars.set("name", "my-project");

        auto result = processor.processContent(input, vars);

        REQUIRE(result == "Class: MyProject, file: my_project.cpp");
    }

    SECTION("handles uppercase transform")
    {
        const std::string input = "{{name|UPPER}}";
        VariableMap vars;
        vars.set("name", "test");

        auto result = processor.processContent(input, vars);

        REQUIRE(result == "TEST");
    }

    SECTION("handles lowercase transform")
    {
        const std::string input = "{{name|lower}}";
        VariableMap vars;
        vars.set("name", "TEST");

        auto result = processor.processContent(input, vars);

        REQUIRE(result == "test");
    }

    SECTION("handles filename processing")
    {
        const std::string input = "{{name}}.cpp";
        VariableMap vars;
        vars.set("name", "test");

        auto result = processor.processFileName(input, vars);

        REQUIRE(result == "test.cpp");
    }

    SECTION("handles empty input")
    {
        const std::string input;
        VariableMap vars;
        vars.set("name", "test");

        auto result = processor.processContent(input, vars);

        REQUIRE(result.empty());
    }

    SECTION("handles special characters in variable values")
    {
        const std::string input = "Path: {{path}}";
        VariableMap vars;
        vars.set("path", "/usr/local/bin");

        auto result = processor.processContent(input, vars);

        REQUIRE(result == "Path: /usr/local/bin");
    }
}
