#pragma once

#include "shared/command/CompositeOperation.h"
#include <memory>

namespace scrap::toolchain {

namespace service {
    class ToolchainService;
}

namespace command {

/**
 * @brief Main toolchain command that manages subcommands
 *
 * This class handles the "scrap toolchain" command and its subcommands
 * (list, install, select) following the Composite pattern.
 */
class ToolchainOperation : public CompositeOperation {
public:
    explicit ToolchainOperation(std::shared_ptr<service::ToolchainService> service);
    ~ToolchainOperation() override = default;

    // Non-copyable
    ToolchainOperation(const ToolchainOperation&) = delete;
    ToolchainOperation& operator=(const ToolchainOperation&) = delete;

    // Movable
    ToolchainOperation(ToolchainOperation&&) = default;
    ToolchainOperation& operator=(ToolchainOperation&&) = default;

    CommandOptions describeOptions() const override;

protected:
    void displayHelp() const override;

private:
    std::shared_ptr<service::ToolchainService> service_;
};

} // namespace command
} // namespace scrap::toolchain
