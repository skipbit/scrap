#include "template/command/TemplateOperation.h"
#include "shared/command/CommandOptions.h"
#include "shared/presentation/Presenter.h"
#include "template/command/ListOperation.h"
#include "template/command/UpdateOperation.h"
#include "template/service/TemplateService.h"

namespace scrap::template_system::command {

class TemplateOperation::Internal {
public:
    explicit Internal(std::shared_ptr<service::TemplateService> service)
        : service_(service)
    {
    }

    std::shared_ptr<service::TemplateService> service_;
};

TemplateOperation::TemplateOperation(std::shared_ptr<service::TemplateService> service)
    : impl_(std::make_unique<Internal>(service))
{
    // Add subcommands
    addSubOperation("list", std::make_shared<ListOperation>(impl_->service_));
    addSubOperation("update", std::make_shared<UpdateOperation>(impl_->service_));
}

TemplateOperation::~TemplateOperation() = default;

TemplateOperation::TemplateOperation(TemplateOperation&&) noexcept = default;

TemplateOperation& TemplateOperation::operator=(TemplateOperation&&) noexcept = default;

CommandOptions TemplateOperation::describeOptions() const
{
    CommandOptions options;
    // CommandOptions uses builder pattern, not direct field assignment
    return options;
}

void TemplateOperation::displayHelp() const
{
    if (presenter()) {
        presenter()->displayInfo("Template Management Commands:");
        presenter()->displayInfo("");
        presenter()->displayInfo("Available subcommands:");
        presenter()->displayInfo("  list     List available templates");
        presenter()->displayInfo("  update   Update template sources");
        presenter()->displayInfo("");
        presenter()->displayInfo(
            "Use 'scrap template <subcommand> --help' for more information about a specific subcommand.");
    }
}

}  // namespace scrap::template_system::command
