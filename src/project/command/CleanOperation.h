#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::project {

namespace service {
class ProjectService;
}

namespace command {

/**
 * @brief Clean operation for removing build artifacts
 *
 * This class handles the "scrap clean" command following
 * Clean Architecture principles with proper separation of concerns.
 */
class CleanOperation : public Operation {
public:
    explicit CleanOperation(std::shared_ptr<service::ProjectService> service);
    ~CleanOperation() override = default;

    // Non-copyable
    CleanOperation(const CleanOperation&) = delete;
    CleanOperation& operator=(const CleanOperation&) = delete;

    // Movable
    CleanOperation(CleanOperation&&) = default;
    CleanOperation& operator=(CleanOperation&&) = default;

    void execute(const std::vector<std::string>& args) override;
    CommandOptions describeOptions() const override;

private:
    std::shared_ptr<service::ProjectService> service_;

    void displayHelp() const;
};

}  // namespace command
}  // namespace scrap::project
