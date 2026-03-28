#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::toolchain {

namespace service {
class ToolchainService;
}

namespace command {

/**
 * @brief List operation for displaying installed toolchains
 *
 * This class handles the "scrap toolchain list" command following
 * Clean Architecture principles with proper separation of concerns.
 */
class ListOperation : public Operation {
public:
    explicit ListOperation(std::shared_ptr<service::ToolchainService> service);
    ~ListOperation() override = default;

    // Non-copyable
    ListOperation(const ListOperation&) = delete;
    ListOperation& operator=(const ListOperation&) = delete;

    // Movable
    ListOperation(ListOperation&&) = default;
    ListOperation& operator=(ListOperation&&) = default;

    void execute(const std::vector<std::string>& args) override;
    CommandOptions describeOptions() const override;

private:
    std::shared_ptr<service::ToolchainService> service_;
};

}  // namespace command
}  // namespace scrap::toolchain
