#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::toolchain {

// Forward declarations
class ToolchainService;

/**
 * @brief List operation for displaying installed toolchains
 * 
 * This class handles the "scrap toolchain list" command following
 * Clean Architecture principles with proper separation of concerns.
 */
class ListOperation : public scrap::Operation {
public:
    explicit ListOperation(std::shared_ptr<ToolchainService> service);
    ~ListOperation() override;
    
    // Non-copyable due to shared_ptr member
    ListOperation(const ListOperation&) = delete;
    ListOperation& operator=(const ListOperation&) = delete;
    
    // Movable
    ListOperation(ListOperation&&) = default;
    ListOperation& operator=(ListOperation&&) = default;

    void execute(const std::vector<std::string>& args) override;

private:
    std::shared_ptr<ToolchainService> service_;
};

}