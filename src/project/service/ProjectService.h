#pragma once

#include "project/model/Project.h"
#include <memory>
#include <optional>

namespace scrap::project::service {

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
    virtual model::Project createNew(const model::ProjectSpecification& spec) = 0;

    /**
     * @brief Load project from current directory or specified path
     * @param path Optional path to project directory
     * @return Project if found, nullopt otherwise
     */
    virtual std::optional<model::Project> loadProject(
        const std::optional<std::filesystem::path>& path = std::nullopt) = 0;

    /**
     * @brief Save project configuration to disk
     * @param project Project to save
     * @throws std::runtime_error if save fails
     */
    virtual void saveProject(const model::Project& project) = 0;

    // Build operations
    /**
     * @brief Build the project
     * @param project Project to build
     * @param options Build options
     * @return Build result
     */
    virtual model::BuildResult build(const model::Project& project,
                                     const model::BuildOptions& options) = 0;

    /**
     * @brief Run the built executable
     * @param project Project to run
     * @param options Run options
     * @throws std::runtime_error if run fails
     */
    virtual void run(const model::Project& project,
                     const model::RunOptions& options) = 0;

    /**
     * @brief Clean build artifacts
     * @param project Project to clean
     */
    virtual void clean(const model::Project& project) = 0;

    // Dependency management
    /**
     * @brief Add dependency to project
     * @param project Project to modify
     * @param dependency Dependency to add
     * @return Modified project
     */
    virtual model::Project addDependency(const model::Project& project,
                                         const model::Dependency& dependency) = 0;
};

/**
 * @brief Mock implementation of ProjectService for testing
 */
class MockProjectService : public ProjectService {
public:
    MockProjectService();
    ~MockProjectService() override = default;

    model::Project createNew(const model::ProjectSpecification& spec) override;
    std::optional<model::Project> loadProject(
        const std::optional<std::filesystem::path>& path = std::nullopt) override;
    void saveProject(const model::Project& project) override;

    model::BuildResult build(const model::Project& project,
                             const model::BuildOptions& options) override;
    void run(const model::Project& project,
             const model::RunOptions& options) override;
    void clean(const model::Project& project) override;

    model::Project addDependency(const model::Project& project,
                                 const model::Dependency& dependency) override;

private:
    void createProjectStructure(const model::Project& project,
                                const std::filesystem::path& basePath);
    void generateSourceFiles(const model::Project& project,
                             const std::filesystem::path& projectPath);
    void generateConfigFile(const model::Project& project,
                            const std::filesystem::path& projectPath);
};

} // namespace scrap::project::service
