#pragma once

#include "template/model/Template.h"
#include <memory>
#include <vector>
#include <optional>
#include <filesystem>

namespace scrap::template_system::service {

using namespace model;

/**
 * @brief Service interface for template management operations
 *
 * This interface defines the business operations for template management,
 * including loading templates from various sources, managing template
 * repositories, and processing templates for project generation.
 */
class TemplateService {
public:
    virtual ~TemplateService() = default;

    // Template discovery and loading
    /**
     * @brief Load a template by name from any available source
     * @param name Template name or source/name format (e.g., "minimal-app" or "custom/web-service")
     * @return Template if found, nullopt otherwise
     */
    virtual std::optional<Template> loadTemplate(const std::string& name) = 0;

    /**
     * @brief Load template from local path
     * @param path Path to template directory
     * @return Template if valid, nullopt otherwise
     */
    virtual std::optional<Template> loadTemplateFromPath(const std::filesystem::path& path) = 0;

    /**
     * @brief List all available templates from all sources
     * @return Vector of all available templates
     */
    virtual std::vector<Template> listAllTemplates() = 0;

    /**
     * @brief List templates from specific source
     * @param sourceName Name of the template source
     * @return Vector of templates from the specified source
     */
    virtual std::vector<Template> listTemplatesFromSource(const std::string& sourceName) = 0;

    // Template source management
    /**
     * @brief Add a new template source
     * @param source Template source configuration
     * @throws std::runtime_error if source cannot be added
     */
    virtual void addTemplateSource(const TemplateSource& source) = 0;

    /**
     * @brief Remove a template source
     * @param sourceName Name of the source to remove
     * @throws std::runtime_error if source cannot be removed
     */
    virtual void removeTemplateSource(const std::string& sourceName) = 0;

    /**
     * @brief List all configured template sources
     * @return Vector of all template sources
     */
    virtual std::vector<TemplateSource> listTemplateSources() = 0;

    /**
     * @brief Update all template sources (git pull for git sources)
     * @throws std::runtime_error if update fails
     */
    virtual void updateTemplateSources() = 0;

    /**
     * @brief Update specific template source
     * @param sourceName Name of the source to update
     * @throws std::runtime_error if update fails
     */
    virtual void updateTemplateSource(const std::string& sourceName) = 0;

    // Template processing
    /**
     * @brief Process a template and generate project files
     * @param tmpl Template to process
     * @param targetPath Target directory for project generation
     * @param variables Variable values for substitution
     * @throws std::runtime_error if processing fails
     */
    virtual void processTemplate(const Template& tmpl,
                               const std::filesystem::path& targetPath,
                               const VariableMap& variables) = 0;

    /**
     * @brief Collect variable values for a template (interactive prompts)
     * @param tmpl Template to collect variables for
     * @param projectName Project name (sets {{name}} variable)
     * @return VariableMap with collected values
     */
    virtual VariableMap collectTemplateVariables(const Template& tmpl,
                                               const std::string& projectName) = 0;

    // Template validation
    /**
     * @brief Validate a template directory
     * @param templatePath Path to template directory
     * @return Vector of validation errors (empty if valid)
     */
    virtual std::vector<std::string> validateTemplate(const std::filesystem::path& templatePath) = 0;

    // Convenience methods
    /**
     * @brief Get recommended template for project type
     * @param projectType Type of project ("app" or "lib")
     * @return Recommended template name, or nullopt if none available
     */
    virtual std::optional<std::string> getRecommendedTemplate(const std::string& projectType) = 0;

    /**
     * @brief Check if template source exists and is accessible
     * @param sourceName Name of the source to check
     * @return true if source is accessible
     */
    virtual bool isTemplateSourceAccessible(const std::string& sourceName) = 0;
};

/**
 * @brief Default implementation of TemplateService
 *
 * Provides concrete implementation of template management operations,
 * including support for official, git, and local template sources.
 */
class DefaultTemplateService : public TemplateService {
public:
    /**
     * @brief Constructor
     * @param templatesDir Base directory for template storage (default: ~/.scrap/templates)
     */
    explicit DefaultTemplateService(
        const std::filesystem::path& templatesDir = getDefaultTemplatesDirectory());

    ~DefaultTemplateService() override = default;

    // Template discovery and loading
    std::optional<Template> loadTemplate(const std::string& name) override;
    std::optional<Template> loadTemplateFromPath(const std::filesystem::path& path) override;
    std::vector<Template> listAllTemplates() override;
    std::vector<Template> listTemplatesFromSource(const std::string& sourceName) override;

    // Template source management
    void addTemplateSource(const TemplateSource& source) override;
    void removeTemplateSource(const std::string& sourceName) override;
    std::vector<TemplateSource> listTemplateSources() override;
    void updateTemplateSources() override;
    void updateTemplateSource(const std::string& sourceName) override;

    // Template processing
    void processTemplate(const Template& tmpl,
                        const std::filesystem::path& targetPath,
                        const VariableMap& variables) override;
    VariableMap collectTemplateVariables(const Template& tmpl,
                                       const std::string& projectName) override;

    // Template validation
    std::vector<std::string> validateTemplate(const std::filesystem::path& templatePath) override;

    // Convenience methods
    std::optional<std::string> getRecommendedTemplate(const std::string& projectType) override;
    bool isTemplateSourceAccessible(const std::string& sourceName) override;

    // Static utility
    static std::filesystem::path getDefaultTemplatesDirectory();

private:
    std::filesystem::path templatesDir_;
    std::filesystem::path registryFile_;

    // Internal helper methods
    void initializeTemplateDirectory();
    void ensureOfficialTemplatesExist();
    void loadTemplateRegistry();
    void saveTemplateRegistry();

    std::filesystem::path getSourceDirectory(const std::string& sourceName);
    std::optional<TemplateSource> findTemplateSource(const std::string& sourceName);

    std::vector<Template> scanTemplatesInDirectory(const std::filesystem::path& dir,
                                                  const TemplateSource& source);

    // Template source management
    std::vector<TemplateSource> templateSources_;
};

} // namespace scrap::template_system::service
