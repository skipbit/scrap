#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::project {

namespace service {
    class ProjectService;
}

namespace command {

/**
 * @brief Run operation for executing built projects
 *
 * This class handles the "scrap run" command following
 * Clean Architecture principles with proper separation of concerns.
 */
class RunOperation : public Operation {
public:
    explicit RunOperation(std::shared_ptr<service::ProjectService> service);
    ~RunOperation() override = default;

    // Non-copyable
    RunOperation(const RunOperation&) = delete;
    RunOperation& operator=(const RunOperation&) = delete;

    // Movable
    RunOperation(RunOperation&&) = default;
    RunOperation& operator=(RunOperation&&) = default;

    void execute(const std::vector<std::string>& args) override;
    CommandOptions describeOptions() const override;

private:
    std::shared_ptr<service::ProjectService> service_;

    void displayHelp() const;
};

} // namespace command
} // namespace scrap::project
