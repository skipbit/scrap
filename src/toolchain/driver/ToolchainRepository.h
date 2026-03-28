#pragma once

#include "toolchain/model/Toolchain.h"
#include <memory>
#include <string>
#include <vector>

namespace scrap::toolchain {

/**
 * @brief Abstract interface for toolchain repository operations
 *
 * This interface defines the contract for toolchain data access,
 * following the Repository pattern and Dependency Inversion Principle.
 */
class ToolchainRepository {
public:
    virtual ~ToolchainRepository() = default;

    /**
     * @brief Get all available toolchains
     * @return Vector of all toolchains
     */
    virtual std::vector<Toolchain> findAll() = 0;

    /**
     * @brief Get the default toolchain
     * @return Default toolchain if available, nullptr otherwise
     */
    virtual std::unique_ptr<Toolchain> findDefault() = 0;

    /**
     * @brief Ensure the toolchain registry is available and updated
     * @return True if registry is successfully updated
     */
    virtual bool ensureRegistryAvailable() = 0;

    /**
     * @brief Get the path to the toolchain registry
     * @return Path to the registry directory
     */
    virtual std::string registryPath() = 0;
};

/**
 * @brief Concrete implementation of ToolchainRepository using Git
 *
 * This class provides a concrete implementation using Git repositories
 * for toolchain registry management, following the implementation details
 * hiding pattern.
 */
class GitToolchainRepository : public ToolchainRepository {
public:
    GitToolchainRepository();
    ~GitToolchainRepository() override;

    // Non-copyable due to PIMPL
    GitToolchainRepository(const GitToolchainRepository&) = delete;
    GitToolchainRepository& operator=(const GitToolchainRepository&) = delete;

    // Movable
    GitToolchainRepository(GitToolchainRepository&&) noexcept;
    GitToolchainRepository& operator=(GitToolchainRepository&&) noexcept;

    std::vector<Toolchain> findAll() override;
    std::unique_ptr<Toolchain> findDefault() override;
    bool ensureRegistryAvailable() override;
    std::string registryPath() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Factory for creating toolchain repositories
 */
class ToolchainRepositoryFactory {
public:
    /**
     * @brief Create a toolchain repository instance
     * @return Shared pointer to toolchain repository
     */
    static std::shared_ptr<ToolchainRepository> createRepository();
};

}  // namespace scrap::toolchain
