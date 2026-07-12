#include "template/service/TemplateService.h"
#include "helpers/FileSystemHelper.h"
#include "template/model/Template.h"
#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace scrap::template_system::service;
using namespace scrap::template_system::model;
using namespace scrap::test;

// Mock implementation for testing
class MockTemplateService : public TemplateService {
private:
    std::vector<Template> templates_;

public:
    MockTemplateService()
    {
        // Add some test templates with proper constructor
        auto tempDir1 = std::filesystem::temp_directory_path() / "mock_template_1";
        auto tempDir2 = std::filesystem::temp_directory_path() / "mock_template_2";

        TemplateSource source1("mock", TemplateSourceType::Local);
        source1.path = tempDir1;
        TemplateSource source2("mock", TemplateSourceType::Local);
        source2.path = tempDir2;

        Template tmpl1("minimal-app", tempDir1, source1);
        tmpl1.setDescription("A minimal C++ application template");
        templates_.push_back(tmpl1);

        Template tmpl2("minimal-lib", tempDir2, source2);
        tmpl2.setDescription("A minimal C++ library template");
        templates_.push_back(tmpl2);
    }

    std::optional<Template> loadTemplate(const std::string& name) override
    {
        for (const auto& tmpl : templates_) {
            if (tmpl.name() == name) {
                return tmpl;
            }
        }
        return std::nullopt;
    }

    std::optional<Template> loadTemplateFromPath(const std::filesystem::path& path) override
    {
        // Simulate loading from path
        TemplateSource source("local", TemplateSourceType::Local);
        source.path = path;

        Template tmpl(path.filename().string(), path, source);
        return tmpl;
    }

    std::vector<Template> listAllTemplates() override
    {
        return templates_;
    }

    std::vector<Template> listTemplatesFromSource(const std::string& sourceName) override
    {
        std::vector<Template> result;
        for (const auto& tmpl : templates_) {
            if (tmpl.source().name == sourceName) {
                result.push_back(tmpl);
            }
        }
        return result;
    }

    std::expected<void, std::string> addTemplateSource([[maybe_unused]] const TemplateSource& source) override
    {
        return {};
    }

    std::expected<void, std::string> removeTemplateSource([[maybe_unused]] const std::string& sourceName) override
    {
        return {};
    }

    std::vector<TemplateSource> listTemplateSources() override
    {
        return {TemplateSource("official", TemplateSourceType::Official),
                TemplateSource("local", TemplateSourceType::Local)};
    }

    std::expected<void, std::string> updateTemplateSources() override
    {
        return {};
    }

    std::expected<void, std::string> updateTemplateSource([[maybe_unused]] const std::string& sourceName) override
    {
        return {};
    }

    std::expected<void, std::string> processTemplate(const Template& tmpl,
                                                     const std::filesystem::path& targetPath,
                                                     [[maybe_unused]] const VariableMap& variables) override
    {

        if (! std::filesystem::exists(targetPath)) {
            std::filesystem::create_directories(targetPath);
        }

        // Create some dummy files
        FileSystemHelper::createFile(targetPath / "main.cpp", "// Generated from template: " + tmpl.name());

        return {};
    }

    VariableMap collectTemplateVariables([[maybe_unused]] const Template& tmpl, const std::string& projectName) override
    {
        VariableMap vars;
        vars.setStandardVariables(projectName);
        return vars;
    }

    std::vector<std::string> validateTemplate(const std::filesystem::path& templatePath) override
    {
        std::vector<std::string> errors;
        if (! std::filesystem::exists(templatePath)) {
            errors.push_back("Template path does not exist");
        }
        return errors;
    }

    std::optional<std::string> recommendedTemplate(const std::string& projectType) override
    {
        if (projectType == "app") {
            return "minimal-app";
        } else if (projectType == "lib") {
            return "minimal-lib";
        }
        return std::nullopt;
    }

    bool isTemplateSourceAccessible(const std::string& sourceName) override
    {
        return sourceName == "official" || sourceName == "local";
    }
};

TEST_CASE("TemplateService operations", "[template][service]")
{
    auto service = std::make_unique<MockTemplateService>();

    SECTION("load template by name")
    {
        SECTION("existing template")
        {
            auto result = service->loadTemplate("minimal-app");

            REQUIRE(result.has_value());
            REQUIRE(result->name() == "minimal-app");
            REQUIRE(result->description() == "A minimal C++ application template");
        }

        SECTION("non-existing template")
        {
            auto result = service->loadTemplate("nonexistent");

            REQUIRE_FALSE(result.has_value());
        }
    }

    SECTION("load template from path")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_service_test_");
        auto templatePath = tempDir.path() / "test-template";
        FileSystemHelper::createDirectories(templatePath);

        auto result = service->loadTemplateFromPath(templatePath);

        REQUIRE(result.has_value());
        REQUIRE(result->name() == "test-template");
        REQUIRE(result->path() == templatePath);
    }

    SECTION("list all templates")
    {
        auto templates = service->listAllTemplates();

        REQUIRE(templates.size() == 2);
        REQUIRE(templates[0].name() == "minimal-app");
        REQUIRE(templates[1].name() == "minimal-lib");
    }

    SECTION("process template")
    {
        auto tempDir = FileSystemHelper::createTempDirectory("template_process_test_");
        auto targetPath = tempDir.path() / "new-project";

        auto tmpl = service->loadTemplate("minimal-app");
        REQUIRE(tmpl.has_value());

        VariableMap vars;
        vars.setStandardVariables("my-project");

        auto result = service->processTemplate(tmpl.value(), targetPath, vars);

        REQUIRE(result.has_value());
        REQUIRE(FileSystemHelper::exists(targetPath / "main.cpp"));

        auto content = FileSystemHelper::readFile(targetPath / "main.cpp");
        REQUIRE(content.find("Generated from template: minimal-app") != std::string::npos);
    }

    SECTION("recommended templates")
    {
        auto appTemplate = service->recommendedTemplate("app");
        REQUIRE(appTemplate.has_value());
        REQUIRE(appTemplate.value() == "minimal-app");

        auto libTemplate = service->recommendedTemplate("lib");
        REQUIRE(libTemplate.has_value());
        REQUIRE(libTemplate.value() == "minimal-lib");

        auto unknownTemplate = service->recommendedTemplate("unknown");
        REQUIRE_FALSE(unknownTemplate.has_value());
    }

    SECTION("template source accessibility")
    {
        REQUIRE(service->isTemplateSourceAccessible("official"));
        REQUIRE(service->isTemplateSourceAccessible("local"));
        REQUIRE_FALSE(service->isTemplateSourceAccessible("nonexistent"));
    }
}
