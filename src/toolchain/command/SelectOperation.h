#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::toolchain {

namespace service {
    class ToolchainService;
}

namespace command {

/**
 * @brief Select operation for choosing the default toolchain
 *
 * This class handles the "scrap toolchain select" command following
 * Clean Architecture principles with proper separation of concerns.
 */
class SelectOperation : public Operation {
public:
    explicit SelectOperation(std::shared_ptr<service::ToolchainService> service);
    ~SelectOperation() override = default;

    // Non-copyable
    SelectOperation(const SelectOperation&) = delete;
    SelectOperation& operator=(const SelectOperation&) = delete;

    // Movable
    SelectOperation(SelectOperation&&) = default;
    SelectOperation& operator=(SelectOperation&&) = default;

    void execute(const std::vector<std::string>& args) override;
    CommandOptions describeOptions() const override;

private:
    std::shared_ptr<service::ToolchainService> service_;

    void displayHelp() const;
};

} // namespace command
} // namespace scrap::toolchain
