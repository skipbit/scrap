#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::toolchain {

namespace service {
    class ToolchainService;
}

namespace command {

/**
 * @brief Install operation for adding new toolchains
 *
 * This class handles the "scrap toolchain install" command following
 * Clean Architecture principles with proper separation of concerns.
 */
class InstallOperation : public Operation {
public:
    explicit InstallOperation(std::shared_ptr<service::ToolchainService> service);
    ~InstallOperation() override = default;

    // Non-copyable
    InstallOperation(const InstallOperation&) = delete;
    InstallOperation& operator=(const InstallOperation&) = delete;

    // Movable
    InstallOperation(InstallOperation&&) = default;
    InstallOperation& operator=(InstallOperation&&) = default;

    void execute(const std::vector<std::string>& args) override;

private:
    std::shared_ptr<service::ToolchainService> service_;

    void displayHelp() const;
};

} // namespace command
} // namespace scrap::toolchain
