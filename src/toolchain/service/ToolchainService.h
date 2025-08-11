#pragma once

#include "toolchain/model/Toolchain.h"
#include <vector>
#include <memory>
#include <optional>

namespace scrap::toolchain::service {

/**
 * @brief Service interface for toolchain management operations
 *
 * This interface defines the business operations available for toolchain
 * management, following Clean Architecture principles.
 */
class ToolchainService {
public:
    virtual ~ToolchainService() = default;

    // Query operations
    /**
     * @brief List all installed toolchains
     * @return Vector of installed toolchains
     */
    virtual std::vector<model::Toolchain> listInstalled() = 0;

    /**
     * @brief Get the currently selected toolchain
     * @return The current toolchain, or nullopt if none selected
     */
    virtual std::optional<model::Toolchain> getCurrentToolchain() = 0;

    /**
     * @brief Find a toolchain by ID
     * @param id Toolchain identifier
     * @return The toolchain if found
     */
    virtual std::optional<model::Toolchain> findById(const model::ToolchainId& id) = 0;

    // Command operations
    /**
     * @brief Install a new toolchain
     * @param spec Toolchain specification
     * @throws std::runtime_error if installation fails
     */
    virtual void install(const model::ToolchainSpecification& spec) = 0;

    /**
     * @brief Select a toolchain as current
     * @param id Toolchain identifier
     * @throws std::runtime_error if selection fails
     */
    virtual void select(const model::ToolchainId& id) = 0;

    /**
     * @brief Remove an installed toolchain
     * @param id Toolchain identifier
     * @throws std::runtime_error if removal fails
     */
    virtual void remove(const model::ToolchainId& id) = 0;
};

/**
 * @brief Mock implementation of ToolchainService for testing
 */
class MockToolchainService : public ToolchainService {
public:
    MockToolchainService();
    ~MockToolchainService() override = default;

    std::vector<model::Toolchain> listInstalled() override;
    std::optional<model::Toolchain> getCurrentToolchain() override;
    std::optional<model::Toolchain> findById(const model::ToolchainId& id) override;
    void install(const model::ToolchainSpecification& spec) override;
    void select(const model::ToolchainId& id) override;
    void remove(const model::ToolchainId& id) override;

private:
    std::vector<model::Toolchain> toolchains_;
    std::optional<model::ToolchainId> currentToolchainId_;

    void initializeMockData();
};

} // namespace scrap::toolchain::service