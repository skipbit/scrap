#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::project {

namespace service {
class ProjectService;
}

}  // namespace scrap::project

namespace scrap::template_system::service {
class TemplateService;
}

namespace scrap::project {

namespace command {

/**
 * @brief New operation for creating new projects
 *
 * This class handles the "scrap new" command following
 * Clean Architecture principles with proper separation of concerns.
 */
class NewOperation : public Operation {
public:
    explicit NewOperation(std::shared_ptr<service::ProjectService> service,
                          std::shared_ptr<template_system::service::TemplateService> templateService = nullptr);
    ~NewOperation() override = default;

    // Non-copyable
    NewOperation(const NewOperation&) = delete;
    NewOperation& operator=(const NewOperation&) = delete;

    // Movable
    NewOperation(NewOperation&&) = default;
    NewOperation& operator=(NewOperation&&) = default;

    void execute(const std::vector<std::string>& args) override;
    CommandOptions describeOptions() const override;
    void execute(const ParsedOptions& options) override;

private:
    std::shared_ptr<service::ProjectService> service_;
    std::shared_ptr<template_system::service::TemplateService> templateService_;
};

}  // namespace command
}  // namespace scrap::project
