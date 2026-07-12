#include "template/command/UpdateOperation.h"
#include "shared/command/CommandOptions.h"
#include "shared/presentation/Presenter.h"
#include "template/service/TemplateService.h"

namespace scrap::template_system::command {

class UpdateOperation::Internal {
public:
    explicit Internal(std::shared_ptr<service::TemplateService> service)
        : service_(service)
    {
    }

    std::shared_ptr<service::TemplateService> service_;
};

UpdateOperation::UpdateOperation(std::shared_ptr<service::TemplateService> service)
    : impl_(std::make_unique<Internal>(service))
{
}

UpdateOperation::~UpdateOperation() = default;

UpdateOperation::UpdateOperation(UpdateOperation&&) noexcept = default;

UpdateOperation& UpdateOperation::operator=(UpdateOperation&&) noexcept = default;

void UpdateOperation::execute(const std::vector<std::string>& args)
{
    if (! impl_->service_) {
        if (presenter()) {
            presenter()->displayError("Template service not available");
        }
        return;
    }

    // Check if a specific source is specified
    if (args.size() > 1) {
        // Update specific source
        updateSpecificSource(args[1]);
    } else {
        // Update all sources
        updateAllSources();
    }
}

CommandOptions UpdateOperation::describeOptions() const
{
    CommandOptions options;
    // CommandOptions uses builder pattern, not direct field assignment
    return options;
}

void UpdateOperation::updateAllSources()
{
    if (presenter()) {
        presenter()->displayInfo("Updating all template sources...");
    }

    auto result = impl_->service_->updateTemplateSources();

    if (result.has_value()) {
        if (presenter()) {
            presenter()->displaySuccess("All template sources updated successfully");
        }
    } else {
        if (presenter()) {
            presenter()->displayError("Failed to update template sources: " + result.error());
        }
    }
}

void UpdateOperation::updateSpecificSource(const std::string& sourceName)
{
    if (presenter()) {
        presenter()->displayInfo("Updating template source: " + sourceName + "...");
    }

    auto result = impl_->service_->updateTemplateSource(sourceName);

    if (result.has_value()) {
        if (presenter()) {
            presenter()->displaySuccess("Template source '" + sourceName + "' updated successfully");
        }
    } else {
        if (presenter()) {
            presenter()->displayError("Failed to update template source '" + sourceName + "': " + result.error());
        }
    }
}

}  // namespace scrap::template_system::command
