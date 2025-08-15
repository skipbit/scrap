#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::project {

namespace service {
    class ProjectService;
}

namespace command {

/**
 * @brief Build operation for compiling projects
 *
 * This class handles the "scrap build" command following
 * Clean Architecture principles with proper separation of concerns.
 */
class BuildOperation : public Operation {
public:
    explicit BuildOperation(std::shared_ptr<service::ProjectService> service);
    ~BuildOperation() override = default;

    // Non-copyable
    BuildOperation(const BuildOperation&) = delete;
    BuildOperation& operator=(const BuildOperation&) = delete;

    // Movable
    BuildOperation(BuildOperation&&) = default;
    BuildOperation& operator=(BuildOperation&&) = default;

    void execute(const std::vector<std::string>& args) override;
    CommandOptions describeOptions() const override;

private:
    std::shared_ptr<service::ProjectService> service_;

    void displayHelp() const;
};

} // namespace command
} // namespace scrap::project
