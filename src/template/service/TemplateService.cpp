#include "TemplateService.h"
#include "TemplateProcessor.h"
#include "repository/driver/GitDriver.h"
#include "shared/presentation/driver/ConsolePresenter.h"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace scrap::template_system::service {

class DefaultTemplateService::Internal {
public:
    std::filesystem::path templatesDir_;
    std::filesystem::path registryFile_;
    std::shared_ptr<repository::GitDriver> gitDriver_;
    std::shared_ptr<Presenter> presenter_;
    std::vector<TemplateSource> templateSources_;

    Internal(const std::filesystem::path& templatesDir,
             std::shared_ptr<repository::GitDriver> gitDriver,
             std::shared_ptr<Presenter> presenter)
        : templatesDir_(templatesDir),
          registryFile_(templatesDir / "registry.toml"),
          gitDriver_(gitDriver ? gitDriver : std::make_shared<repository::GitDriver>()),
          presenter_(presenter ? presenter : std::make_shared<ConsolePresenter>()) {
        initializeTemplateDirectory();
        loadTemplateRegistry();
        // Ignore errors during initialization - templates can be cloned on demand
        auto result = ensureOfficialTemplatesExist();
        if (!result) {
            presenter_->displayWarning(result.error());
        }
    }

    // Internal helper methods
    void initializeTemplateDirectory() {
        std::filesystem::create_directories(templatesDir_);
        std::filesystem::create_directories(templatesDir_ / "official");
        std::filesystem::create_directories(templatesDir_ / "user");
    }

    std::expected<void, std::string> ensureOfficialTemplatesExist() {
        // Check if official templates source is configured
        auto officialSource = findTemplateSource("official");
        if (!officialSource) {
            // Add official template source
            auto official = TemplateSource("official", TemplateSourceType::Git);
            official.url = "https://github.com/skipbit/scrap-templates.git";
            official.autoUpdate = true;

            templateSources_.push_back(official);
            saveTemplateRegistry();
            officialSource = findTemplateSource("official");
        }

        // Check if official templates are cloned
        if (officialSource && officialSource->type == TemplateSourceType::Git && officialSource->url) {
            auto targetDir = getSourceDirectory("official");

            // Clone if directory doesn't exist
            if (!std::filesystem::exists(targetDir)) {
                try {
                    // Create parent directory if needed
                    std::filesystem::create_directories(targetDir.parent_path());

                    // Clone the repository
                    gitDriver_->clone(*officialSource->url, targetDir);

                    presenter_->displaySuccess("Successfully cloned official templates from " + *officialSource->url);
                } catch (const std::exception& e) {
                    return std::unexpected("Failed to clone official templates: " + std::string(e.what()));
                }
            }
        }

        return {};
    }

    void loadTemplateRegistry() {
        if (!std::filesystem::exists(registryFile_)) {
            return;
        }

        // TODO: Implement TOML parsing when dross support is ready
        // For now, start with empty registry
        templateSources_.clear();
    }

    void saveTemplateRegistry() {
        std::ofstream registry(registryFile_);
        if (!registry) {
            throw std::runtime_error("Cannot write template registry");
        }

        // TODO: Implement TOML serialization when dross support is ready
        // For now, write a simple format
        registry << "# Template Sources Registry\n";
        registry << "# This file is managed by scrap\n\n";

        for (const auto& source : templateSources_) {
            registry << "[[sources]]\n";
            registry << "name = \"" << source.name << "\"\n";
            registry << "type = \"" << templateSourceTypeToString(source.type) << "\"\n";

            if (source.url) {
                registry << "url = \"" << *source.url << "\"\n";
            }

            if (source.path) {
                registry << "path = \"" << source.path->string() << "\"\n";
            }

            registry << "branch = \"" << source.branch << "\"\n";
            registry << "auto_update = " << (source.autoUpdate ? "true" : "false") << "\n";
            registry << "\n";
        }
    }

    std::filesystem::path getSourceDirectory(const std::string& sourceName) {
        if (sourceName == "official") {
            return templatesDir_ / "official" / "scrap-templates";
        }
        return templatesDir_ / "user" / sourceName;
    }

    std::optional<TemplateSource> findTemplateSource(const std::string& sourceName) {
        auto it = std::find_if(templateSources_.begin(), templateSources_.end(),
                              [&sourceName](const TemplateSource& source) {
                                  return source.name == sourceName;
                              });

        if (it != templateSources_.end()) {
            return *it;
        }

        return std::nullopt;
    }

    std::vector<Template> scanTemplatesInDirectory(
        const std::filesystem::path& dir, const TemplateSource& source) {

        std::vector<Template> templates;

        if (!std::filesystem::exists(dir)) {
            return templates;
        }

        std::error_code ec;
        for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) continue;

            if (entry.is_directory()) {
                auto templateName = entry.path().filename().string();

                // Skip hidden directories
                if (templateName.starts_with('.')) {
                    continue;
                }

                try {
                    auto tmpl = Template(templateName, entry.path(), source);
                    if (tmpl.isValid()) {
                        templates.push_back(std::move(tmpl));
                    }
                } catch (const std::exception&) {
                    // Skip invalid templates
                }
            }
        }

        return templates;
    }
};

DefaultTemplateService::DefaultTemplateService(const std::filesystem::path& templatesDir,
                                             std::shared_ptr<repository::GitDriver> gitDriver,
                                             std::shared_ptr<Presenter> presenter)
    : impl_(std::make_unique<Internal>(templatesDir, gitDriver, presenter)) {
}

DefaultTemplateService::~DefaultTemplateService() = default;

std::optional<Template> DefaultTemplateService::loadTemplate(const std::string& name) {
    // Handle source/name format (e.g., "custom/web-service")
    std::string sourceName = "official";  // default source
    std::string templateName = name;

    auto slashPos = name.find('/');
    if (slashPos != std::string::npos) {
        sourceName = name.substr(0, slashPos);
        templateName = name.substr(slashPos + 1);
    }

    // Find the template source
    auto source = impl_->findTemplateSource(sourceName);
    if (!source) {
        return std::nullopt;
    }

    // Construct template path
    std::filesystem::path templatePath;
    if (source->type == TemplateSourceType::Local && source->path) {
        templatePath = *source->path / templateName;
    } else {
        templatePath = impl_->getSourceDirectory(sourceName) / templateName;
    }

    return loadTemplateFromPath(templatePath);
}

std::optional<Template> DefaultTemplateService::loadTemplateFromPath(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
        return std::nullopt;
    }

    // Create a temporary source for path-based templates
    auto source = TemplateSource("local", TemplateSourceType::Local);
    source.path = path.parent_path();

    auto templateName = path.filename().string();

    try {
        return Template(templateName, path, source);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<Template> DefaultTemplateService::listAllTemplates() {
    std::vector<Template> allTemplates;

    for (const auto& source : impl_->templateSources_) {
        auto templates = listTemplatesFromSource(source.name);
        allTemplates.insert(allTemplates.end(), templates.begin(), templates.end());
    }

    return allTemplates;
}

std::vector<Template> DefaultTemplateService::listTemplatesFromSource(const std::string& sourceName) {
    auto source = impl_->findTemplateSource(sourceName);
    if (!source) {
        return {};
    }

    std::filesystem::path sourceDir;
    if (source->type == TemplateSourceType::Local && source->path) {
        sourceDir = *source->path;
    } else {
        sourceDir = impl_->getSourceDirectory(sourceName);
    }

    return impl_->scanTemplatesInDirectory(sourceDir, *source);
}

std::expected<void, std::string> DefaultTemplateService::addTemplateSource(const TemplateSource& source) {
    // Check if source already exists
    auto existing = impl_->findTemplateSource(source.name);
    if (existing) {
        return std::unexpected("Template source already exists: " + source.name);
    }

    // TODO: Validate source accessibility

    impl_->templateSources_.push_back(source);
    impl_->saveTemplateRegistry();

    // If it's a git source, clone it
    if (source.type == TemplateSourceType::Git && source.url) {
        auto targetDir = impl_->getSourceDirectory(source.name);

        // Check if directory already exists
        if (std::filesystem::exists(targetDir)) {
            // Directory exists, skip cloning
            return {};
        }

        try {
            // Create parent directory if needed
            std::filesystem::create_directories(targetDir.parent_path());

            // Clone the repository
            impl_->gitDriver_->clone(*source.url, targetDir);

            impl_->presenter_->displaySuccess("Successfully cloned templates from " + *source.url);
        } catch (const std::exception& e) {
            impl_->presenter_->displayError("Failed to clone template repository: " + std::string(e.what()));
            return std::unexpected("Failed to clone template repository from " + *source.url + ": " + e.what());
        }
    }

    return {};
}

std::expected<void, std::string> DefaultTemplateService::removeTemplateSource(const std::string& sourceName) {
    if (sourceName == "official") {
        return std::unexpected("Cannot remove official template source");
    }

    auto it = std::find_if(impl_->templateSources_.begin(), impl_->templateSources_.end(),
                          [&sourceName](const TemplateSource& source) {
                              return source.name == sourceName;
                          });

    if (it == impl_->templateSources_.end()) {
        return std::unexpected("Template source not found: " + sourceName);
    }

    // Remove directory if it exists
    auto sourceDir = impl_->getSourceDirectory(sourceName);
    if (std::filesystem::exists(sourceDir)) {
        std::filesystem::remove_all(sourceDir);
    }

    impl_->templateSources_.erase(it);
    impl_->saveTemplateRegistry();

    return {};
}

std::vector<TemplateSource> DefaultTemplateService::listTemplateSources() {
    return impl_->templateSources_;
}

std::expected<void, std::string> DefaultTemplateService::updateTemplateSources() {
    std::string errors;
    bool hasErrors = false;

    for (const auto& source : impl_->templateSources_) {
        if (source.autoUpdate) {
            auto result = updateTemplateSource(source.name);
            if (!result) {
                hasErrors = true;
                errors += "Failed to update template source '" + source.name + "': " + result.error() + "; ";
                impl_->presenter_->displayError("Failed to update template source '" + source.name + "': " + result.error());
            }
        }
    }

    if (hasErrors) {
        return std::unexpected(errors);
    }

    return {};
}

std::expected<void, std::string> DefaultTemplateService::updateTemplateSource(const std::string& sourceName) {
    auto source = impl_->findTemplateSource(sourceName);
    if (!source) {
        return std::unexpected("Template source not found: " + sourceName);
    }

    if (source->type == TemplateSourceType::Git) {
        auto sourceDir = impl_->getSourceDirectory(sourceName);

        // Check if directory exists
        if (!std::filesystem::exists(sourceDir)) {
            // If not cloned yet, clone it now
            if (source->url) {
                try {
                    std::filesystem::create_directories(sourceDir.parent_path());
                    impl_->gitDriver_->clone(*source->url, sourceDir);
                    impl_->presenter_->displaySuccess("Successfully cloned template source '" + sourceName + "'");
                } catch (const std::exception& e) {
                    return std::unexpected("Failed to clone template source: " + std::string(e.what()));
                }
            } else {
                return std::unexpected("Git source has no URL configured");
            }
        } else {
            // Directory exists, perform update
            try {
                impl_->gitDriver_->update(sourceDir);
                impl_->presenter_->displaySuccess("Successfully updated template source '" + sourceName + "'");
            } catch (const std::exception& e) {
                return std::unexpected("Failed to update template source: " + std::string(e.what()));
            }
        }
    }
    // Local sources don't need updating

    return {};
}

std::expected<void, std::string> DefaultTemplateService::processTemplate(const Template& tmpl,
                                            const std::filesystem::path& targetPath,
                                            const VariableMap& variables) {
    try {
        // Use the advanced TemplateProcessor for proper processing
        TemplateProcessor processor;
        processor.processTemplateDirectory(tmpl.path(), targetPath, variables);
        return {};
    } catch (const std::exception& e) {
        return std::unexpected("Failed to process template: " + std::string(e.what()));
    }
}

VariableMap DefaultTemplateService::collectTemplateVariables(const Template& tmpl,
                                                           const std::string& projectName) {
    VariableMap variables;
    variables.setStandardVariables(projectName);

    // For now, just use defaults for all template variables
    // TODO: Implement interactive prompts
    for (const auto& var : tmpl.variables()) {
        if (var.defaultValue) {
            variables.set(var.name, *var.defaultValue);
        } else if (var.type == TemplateVariable::Type::Boolean) {
            variables.set(var.name, "false");
        } else {
            variables.set(var.name, "");
        }
    }

    return variables;
}

std::vector<std::string> DefaultTemplateService::validateTemplate(const std::filesystem::path& templatePath) {
    if (!std::filesystem::exists(templatePath)) {
        return {"Template path does not exist"};
    }

    // Load template and validate
    auto tmpl = loadTemplateFromPath(templatePath);
    if (!tmpl) {
        return {"Cannot load template from path"};
    }

    return tmpl->validate();
}

std::optional<std::string> DefaultTemplateService::getRecommendedTemplate(const std::string& projectType) {
    if (projectType == "app" || projectType == "application") {
        // Check if minimal-app template exists
        auto tmpl = loadTemplate("minimal-app");
        if (tmpl) {
            return "minimal-app";
        }
    }

    if (projectType == "lib" || projectType == "library") {
        // Check if minimal-lib template exists
        auto tmpl = loadTemplate("minimal-lib");
        if (tmpl) {
            return "minimal-lib";
        }
    }

    return std::nullopt;
}

bool DefaultTemplateService::isTemplateSourceAccessible(const std::string& sourceName) {
    auto source = impl_->findTemplateSource(sourceName);
    if (!source) {
        return false;
    }

    if (source->type == TemplateSourceType::Local && source->path) {
        return std::filesystem::exists(*source->path);
    }

    auto sourceDir = impl_->getSourceDirectory(sourceName);
    return std::filesystem::exists(sourceDir);
}

std::filesystem::path DefaultTemplateService::getDefaultTemplatesDirectory() {
    // Check SCRAP_HOME environment variable first
    const char* scrapHome = std::getenv("SCRAP_HOME");
    if (scrapHome) {
        return std::filesystem::path(scrapHome) / "templates";
    }

    // Fall back to ~/.scrap/templates
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE"); // Windows
    }

    if (!home) {
        throw std::runtime_error("Cannot determine home directory");
    }

    return std::filesystem::path(home) / ".scrap" / "templates";
}


} // namespace scrap::template_system::service
