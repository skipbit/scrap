#pragma once

#include "repository/model/Repository.h"
#include <filesystem>
#include <memory>

namespace scrap::repository {

/**
 * @brief Factory for creating Repository instances with appropriate drivers
 *
 * This factory encapsulates the infrastructure dependency (GitDriver)
 * and provides a clean interface for creating repositories without
 * violating the Dependency Inversion Principle.
 */
class RepositoryFactory {
public:
    /**
     * @brief Create a Git repository with GitDriver
     * @param path Repository directory path
     * @return Unique pointer to Repository
     */
    static std::unique_ptr<Repository> createGitRepository(const std::filesystem::path& path);
};

}  // namespace scrap::repository
