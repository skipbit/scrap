#pragma once

#include "toolchain/model/Toolchain.h"
#include <vector>
#include <memory>
#include <string>

namespace scrap::toolchain {

// Forward declarations
class ToolchainRepository;

/**
 * @brief Service class for toolchain operations following DDD principles
 * 
 * This class encapsulates the use cases and business logic for toolchain management.
 * It coordinates between the domain model and the infrastructure layer.
 */
class ToolchainService {
public:
    explicit ToolchainService(std::shared_ptr<ToolchainRepository> repository);
    ~ToolchainService() = default;
    
    // Non-copyable due to shared_ptr member
    ToolchainService(const ToolchainService&) = delete;
    ToolchainService& operator=(const ToolchainService&) = delete;
    
    // Movable
    ToolchainService(ToolchainService&&) = default;
    ToolchainService& operator=(ToolchainService&&) = default;
    
    /**
     * @brief Get all installed toolchains
     * @return Vector of all installed toolchains
     */
    std::vector<Toolchain> getAllToolchains();
    
    /**
     * @brief Get the currently selected default toolchain
     * @return Default toolchain if available, nullptr otherwise
     */
    std::unique_ptr<Toolchain> getDefaultToolchain();
    
    /**
     * @brief Ensure toolchain registry is available and up-to-date
     * @return True if registry is successfully updated
     */
    bool ensureRegistryUpToDate();

private:
    std::shared_ptr<ToolchainRepository> repository_;
};

}