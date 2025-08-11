#include "TemplateService.h"
#include "TemplateProcessor.h"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace scrap::template_system::service {

DefaultTemplateService::DefaultTemplateService(const std::filesystem::path& templatesDir)
    : templatesDir_(templatesDir), registryFile_(templatesDir / "registry.toml") {
    initializeTemplateDirectory();
    loadTemplateRegistry();
    ensureOfficialTemplatesExist();
}

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
    auto source = findTemplateSource(sourceName);
    if (!source) {
        return std::nullopt;
    }

    // Construct template path
    std::filesystem::path templatePath;
    if (source->type == TemplateSourceType::Local && source->path) {
        templatePath = *source->path / templateName;
    } else {
        templatePath = getSourceDirectory(sourceName) / templateName;
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

    for (const auto& source : templateSources_) {
        auto templates = listTemplatesFromSource(source.name);
        allTemplates.insert(allTemplates.end(), templates.begin(), templates.end());
    }

    return allTemplates;
}

std::vector<Template> DefaultTemplateService::listTemplatesFromSource(const std::string& sourceName) {
    auto source = findTemplateSource(sourceName);
    if (!source) {
        return {};
    }

    std::filesystem::path sourceDir;
    if (source->type == TemplateSourceType::Local && source->path) {
        sourceDir = *source->path;
    } else {
        sourceDir = getSourceDirectory(sourceName);
    }

    return scanTemplatesInDirectory(sourceDir, *source);
}

void DefaultTemplateService::addTemplateSource(const TemplateSource& source) {
    // Check if source already exists
    auto existing = findTemplateSource(source.name);
    if (existing) {
        throw std::runtime_error("Template source already exists: " + source.name);
    }

    // TODO: Validate source accessibility

    templateSources_.push_back(source);
    saveTemplateRegistry();

    // If it's a git source, clone it
    if (source.type == TemplateSourceType::Git && source.url) {
        // TODO: Implement git cloning using repository::GitDriver
        std::cout << "Would clone " << *source.url << " to " << getSourceDirectory(source.name) << std::endl;
    }
}

void DefaultTemplateService::removeTemplateSource(const std::string& sourceName) {
    if (sourceName == "official") {
        throw std::runtime_error("Cannot remove official template source");
    }

    auto it = std::find_if(templateSources_.begin(), templateSources_.end(),
                          [&sourceName](const TemplateSource& source) {
                              return source.name == sourceName;
                          });

    if (it == templateSources_.end()) {
        throw std::runtime_error("Template source not found: " + sourceName);
    }

    // Remove directory if it exists
    auto sourceDir = getSourceDirectory(sourceName);
    if (std::filesystem::exists(sourceDir)) {
        std::filesystem::remove_all(sourceDir);
    }

    templateSources_.erase(it);
    saveTemplateRegistry();
}

std::vector<TemplateSource> DefaultTemplateService::listTemplateSources() {
    return templateSources_;
}

void DefaultTemplateService::updateTemplateSources() {
    for (const auto& source : templateSources_) {
        if (source.autoUpdate) {
            try {
                updateTemplateSource(source.name);
            } catch (const std::exception& e) {
                std::cerr << "Failed to update template source '" << source.name
                         << "': " << e.what() << std::endl;
            }
        }
    }
}

void DefaultTemplateService::updateTemplateSource(const std::string& sourceName) {
    auto source = findTemplateSource(sourceName);
    if (!source) {
        throw std::runtime_error("Template source not found: " + sourceName);
    }

    if (source->type == TemplateSourceType::Git) {
        // TODO: Implement git pull using repository::GitDriver
        auto sourceDir = getSourceDirectory(sourceName);
        std::cout << "Would update git repository at " << sourceDir << std::endl;
    }
    // Local sources don't need updating
}

void DefaultTemplateService::processTemplate(const Template& tmpl,
                                            const std::filesystem::path& targetPath,
                                            const VariableMap& variables) {
    // Use the advanced TemplateProcessor for proper processing
    TemplateProcessor processor;
    processor.processTemplateDirectory(tmpl.getPath(), targetPath, variables);
}

VariableMap DefaultTemplateService::collectTemplateVariables(const Template& tmpl,
                                                           const std::string& projectName) {
    VariableMap variables;
    variables.setStandardVariables(projectName);

    // For now, just use defaults for all template variables
    // TODO: Implement interactive prompts
    for (const auto& var : tmpl.getVariables()) {
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
    auto source = findTemplateSource(sourceName);
    if (!source) {
        return false;
    }

    if (source->type == TemplateSourceType::Local && source->path) {
        return std::filesystem::exists(*source->path);
    }

    auto sourceDir = getSourceDirectory(sourceName);
    return std::filesystem::exists(sourceDir);
}

std::filesystem::path DefaultTemplateService::getDefaultTemplatesDirectory() {
    // Use ~/.scrap/templates
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE"); // Windows
    }

    if (!home) {
        throw std::runtime_error("Cannot determine home directory");
    }

    return std::filesystem::path(home) / ".scrap" / "templates";
}

void DefaultTemplateService::initializeTemplateDirectory() {
    std::filesystem::create_directories(templatesDir_);
    std::filesystem::create_directories(templatesDir_ / "official");
    std::filesystem::create_directories(templatesDir_ / "user");
}

void DefaultTemplateService::ensureOfficialTemplatesExist() {
    // Check if official templates source is configured
    auto officialSource = findTemplateSource("official");
    if (!officialSource) {
        // Add official template source
        auto official = TemplateSource("official", TemplateSourceType::Git);
        official.url = "https://github.com/skipbit/scrap-templates.git";
        official.autoUpdate = true;

        templateSources_.push_back(official);
        saveTemplateRegistry();
    }
}

void DefaultTemplateService::loadTemplateRegistry() {
    if (!std::filesystem::exists(registryFile_)) {
        return;
    }

    // TODO: Implement TOML parsing when dross support is ready
    // For now, start with empty registry
    templateSources_.clear();
}

void DefaultTemplateService::saveTemplateRegistry() {
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

std::filesystem::path DefaultTemplateService::getSourceDirectory(const std::string& sourceName) {
    if (sourceName == "official") {
        return templatesDir_ / "official" / "scrap-templates";
    }
    return templatesDir_ / "user" / sourceName;
}

std::optional<TemplateSource> DefaultTemplateService::findTemplateSource(const std::string& sourceName) {
    auto it = std::find_if(templateSources_.begin(), templateSources_.end(),
                          [&sourceName](const TemplateSource& source) {
                              return source.name == sourceName;
                          });

    if (it != templateSources_.end()) {
        return *it;
    }

    return std::nullopt;
}

std::vector<Template> DefaultTemplateService::scanTemplatesInDirectory(
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

} // namespace scrap::template_system::service
