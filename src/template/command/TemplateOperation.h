#pragma once

#include "shared/command/CompositeOperation.h"
#include <memory>

namespace scrap::template_system {

namespace service {
    class TemplateService;
}

namespace command {

/**
 * @brief Main template command that manages subcommands
 *
 * This class handles the "scrap template" command and its subcommands
 * (list, update) following the Composite pattern.
 */
class TemplateOperation : public CompositeOperation {
public:
    explicit TemplateOperation(std::shared_ptr<service::TemplateService> service);
    ~TemplateOperation() override;

    // Non-copyable
    TemplateOperation(const TemplateOperation&) = delete;
    TemplateOperation& operator=(const TemplateOperation&) = delete;

    // Movable
    TemplateOperation(TemplateOperation&&) noexcept;
    TemplateOperation& operator=(TemplateOperation&&) noexcept;

    CommandOptions describeOptions() const override;

protected:
    void displayHelp() const override;

private:
    class Internal;
    std::unique_ptr<Internal> impl_;
};

} // namespace command
} // namespace scrap::template_system
