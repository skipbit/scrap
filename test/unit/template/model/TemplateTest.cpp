#include "template/model/Template.h"
#include "helpers/FileSystemHelper.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace scrap::template_system::model;
using namespace scrap::test;

TEST_CASE("Template model validation and properties", "[template][model]")
{

    SECTION("creates valid template with required fields")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        const Template tmpl("test-template", tempDir.path(), source);

        REQUIRE(tmpl.name() == "test-template");
        REQUIRE(tmpl.isValid());
    }

    SECTION("template with description")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        Template tmpl("test-template", tempDir.path(), source);
        tmpl.setDescription("A template for testing");

        REQUIRE(tmpl.description() == "A template for testing");
        REQUIRE(tmpl.isValid());
    }

    SECTION("template with version")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        Template tmpl("test-template", tempDir.path(), source);
        tmpl.setVersion("1.0.0");

        REQUIRE(tmpl.version() == "1.0.0");
        REQUIRE(tmpl.isValid());
    }

    SECTION("template with author")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        Template tmpl("test-template", tempDir.path(), source);
        tmpl.setAuthor("Test Author");

        REQUIRE(tmpl.author() == "Test Author");
        REQUIRE(tmpl.isValid());
    }

    SECTION("template with source path")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        const Template tmpl("test-template", tempDir.path(), source);

        REQUIRE(tmpl.path() == tempDir.path());
        REQUIRE(tmpl.isValid());
    }

    SECTION("template with variables")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        Template tmpl("test-template", tempDir.path(), source);

        TemplateVariable var1("project_name", "Project Name");
        var1.defaultValue = "my-project";
        var1.required = true;

        TemplateVariable var2("author", "Author");
        var2.required = false;

        tmpl.addVariable(var1);
        tmpl.addVariable(var2);

        REQUIRE(tmpl.variables().size() == 2);
        REQUIRE(tmpl.variables()[0].name == "project_name");
        REQUIRE(tmpl.variables()[0].required == true);
        REQUIRE(tmpl.variables()[1].name == "author");
        REQUIRE(tmpl.variables()[1].required == false);
        REQUIRE(tmpl.isValid());
    }

    SECTION("template with tags")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        Template tmpl("test-template", tempDir.path(), source);
        tmpl.addTag("cpp");
        tmpl.addTag("library");
        tmpl.addTag("modern");

        auto tags = tmpl.tags();
        REQUIRE(tags.size() == 3);
        REQUIRE(std::ranges::find(tags, "cpp") != tags.end());
        REQUIRE(std::ranges::find(tags, "library") != tags.end());
        REQUIRE(std::ranges::find(tags, "modern") != tags.end());
        REQUIRE(tmpl.isValid());
    }

    SECTION("invalid template - empty name")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        const Template tmpl("", tempDir.path(), source);
        REQUIRE_FALSE(tmpl.isValid());
    }

    SECTION("template basic properties")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        const Template tmpl1("test-template", tempDir.path(), source);
        const Template tmpl2("test-template", tempDir.path(), source);
        const Template tmpl3("other-template", tempDir.path(), source);

        REQUIRE(tmpl1.name() == tmpl2.name());
        REQUIRE(tmpl1.name() != tmpl3.name());
        REQUIRE(tmpl1.path() == tempDir.path());
        REQUIRE(tmpl1.source().name == "test-source");
    }

    SECTION("template with dependencies")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_test_");
        TemplateSource source("test-source", TemplateSourceType::Local);
        source.path = tempDir.path();

        Template tmpl("test-template", tempDir.path(), source);
        tmpl.addDefaultDependency("cmake", "3.20");
        tmpl.addDefaultDependency("gcc", "11.0");

        auto deps = tmpl.defaultDependencies();
        REQUIRE(deps.size() == 2);
        REQUIRE(deps.at("cmake") == "3.20");
        REQUIRE(deps.at("gcc") == "11.0");
    }
}
