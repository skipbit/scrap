#pragma once

#include "shared/command/Operation.h"
#include <memory>

namespace scrap::template_system {

namespace service {
    class TemplateService;
}

namespace command {

/**
 * @brief Operation for updating template sources
 *
 * This class handles the "scrap template update" command, updating
 * all configured template sources (e.g., git pull for git repositories).
 */
class UpdateOperation : public Operation {
public:
    explicit UpdateOperation(std::shared_ptr<service::TemplateService> service);
    ~UpdateOperation() override;

    // Non-copyable
    UpdateOperation(const UpdateOperation&) = delete;
    UpdateOperation& operator=(const UpdateOperation&) = delete;

    // Movable
    UpdateOperation(UpdateOperation&&) noexcept;
    UpdateOperation& operator=(UpdateOperation&&) noexcept;

    void execute(const std::vector<std::string>& args) override;
    CommandOptions describeOptions() const override;

private:
    class Internal;
    std::unique_ptr<Internal> impl_;

    void updateAllSources();
    void updateSpecificSource(const std::string& sourceName);
};

} // namespace command
} // namespace scrap::template_system
