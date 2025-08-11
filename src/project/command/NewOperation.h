#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::project {

namespace service {
    class ProjectService;
}

namespace command {

/**
 * @brief New operation for creating new projects
 *
 * This class handles the "scrap new" command following
 * Clean Architecture principles with proper separation of concerns.
 */
class NewOperation : public Operation {
public:
    explicit NewOperation(std::shared_ptr<service::ProjectService> service);
    ~NewOperation() override = default;

    // Non-copyable
    NewOperation(const NewOperation&) = delete;
    NewOperation& operator=(const NewOperation&) = delete;

    // Movable
    NewOperation(NewOperation&&) = default;
    NewOperation& operator=(NewOperation&&) = default;

    void execute(const std::vector<std::string>& args) override;

private:
    std::shared_ptr<service::ProjectService> service_;

    void displayHelp() const;
};

} // namespace command
} // namespace scrap::project
