#pragma once

#include "project/model/Project.h"
#include <memory>
#include <optional>

namespace scrap {
class Presenter;
}

namespace scrap::template_system::service {
class TemplateService;
}

namespace scrap::project::service {

// Namespace alias for cleaner code
namespace Model = scrap::Project::Model;

/**
 * @brief Service interface for project management operations
 *
 * This interface defines the business operations available for project
 * management, following Clean Architecture principles.
 */
class ProjectService {
public:
    virtual ~ProjectService() = default;

    // Project lifecycle operations
    /**
     * @brief Create a new project from specification
     * @param spec Project creation specification
     * @return Created project
     * @throws std::runtime_error if creation fails
     */
    virtual Model::Project createNew(const Model::ProjectSpecification& spec) = 0;

    /**
     * @brief Load project from current directory or specified path
     * @param path Optional path to project directory
     * @return Project if found, nullopt otherwise
     */
    virtual std::optional<Model::Project>
    loadProject(const std::optional<std::filesystem::path>& path = std::nullopt) = 0;

    /**
     * @brief Save project configuration to disk
     * @param project Project to save
     * @throws std::runtime_error if save fails
     */
    virtual void saveProject(const Model::Project& project) = 0;

    // Build operations
    /**
     * @brief Build the project
     * @param project Project to build
     * @param options Build options
     * @return Build result
     */
    virtual Model::BuildResult build(const Model::Project& project, const Model::BuildOptions& options) = 0;

    /**
     * @brief Run the built executable
     * @param project Project to run
     * @param options Run options
     * @throws std::runtime_error if run fails
     */
    virtual void run(const Model::Project& project, const Model::RunOptions& options) = 0;

    /**
     * @brief Clean build artifacts
     * @param project Project to clean
     */
    virtual void clean(const Model::Project& project) = 0;

    // Dependency management
    /**
     * @brief Add dependency to project
     * @param project Project to modify
     * @param dependency Dependency to add
     * @return Modified project
     */
    virtual Model::Project addDependency(const Model::Project& project, const Model::Dependency& dependency) = 0;
};

/**
 * @brief Mock implementation of ProjectService for testing
 */
class MockProjectService : public ProjectService {
public:
    MockProjectService();
    explicit MockProjectService(std::shared_ptr<template_system::service::TemplateService> templateService = nullptr,
                                std::shared_ptr<Presenter> presenter = nullptr);
    ~MockProjectService() override = default;

    Model::Project createNew(const Model::ProjectSpecification& spec) override;
    std::optional<Model::Project> loadProject(const std::optional<std::filesystem::path>& path = std::nullopt) override;
    void saveProject(const Model::Project& project) override;

    Model::BuildResult build(const Model::Project& project, const Model::BuildOptions& options) override;
    void run(const Model::Project& project, const Model::RunOptions& options) override;
    void clean(const Model::Project& project) override;

    Model::Project addDependency(const Model::Project& project, const Model::Dependency& dependency) override;

private:
    std::shared_ptr<template_system::service::TemplateService> templateService_;
    std::shared_ptr<Presenter> presenter_;

    void createProjectStructure(const Model::Project& project, const std::filesystem::path& basePath);
    void generateSourceFiles(const Model::Project& project, const std::filesystem::path& projectPath);
    void generateConfigFile(const Model::Project& project, const std::filesystem::path& projectPath);
    void createProjectFromTemplate(const Model::ProjectSpecification& spec, const std::filesystem::path& targetPath);
};

}  // namespace scrap::project::service
