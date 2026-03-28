#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::template_system {

namespace service {
class TemplateService;
}

namespace command {

/**
 * @brief Operation for listing available templates
 *
 * This class handles the "scrap template list" command, displaying
 * all available templates from all configured sources.
 */
class ListOperation : public Operation {
public:
    explicit ListOperation(std::shared_ptr<service::TemplateService> service);
    ~ListOperation() override;

    // Non-copyable
    ListOperation(const ListOperation&) = delete;
    ListOperation& operator=(const ListOperation&) = delete;

    // Movable
    ListOperation(ListOperation&&) noexcept;
    ListOperation& operator=(ListOperation&&) noexcept;

    void execute(const std::vector<std::string>& args) override;
    CommandOptions describeOptions() const override;

private:
    class Internal;
    std::unique_ptr<Internal> impl_;

    void displayTemplateList();
    void displayTemplatesBySource();
};

}  // namespace command
}  // namespace scrap::template_system
