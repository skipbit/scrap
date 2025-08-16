#include "template/command/ListOperation.h"
#include "template/service/TemplateService.h"
#include "shared/command/CommandOptions.h"
#include "shared/presentation/Presenter.h"

namespace scrap::template_system::command {

class ListOperation::Internal {
public:
    explicit Internal(std::shared_ptr<service::TemplateService> service)
        : service_(service)
    {
    }

    std::shared_ptr<service::TemplateService> service_;
};

ListOperation::ListOperation(std::shared_ptr<service::TemplateService> service)
    : impl_(std::make_unique<Internal>(service))
{
}

ListOperation::~ListOperation() = default;

ListOperation::ListOperation(ListOperation&&) noexcept = default;

ListOperation& ListOperation::operator=(ListOperation&&) noexcept = default;

void ListOperation::execute(const std::vector<std::string>& /* args */)
{
    if (!impl_->service_) {
        if (presenter()) {
            presenter()->displayError("Template service not available");
        }
        return;
    }

    // For now, display all templates grouped by source
    displayTemplatesBySource();
}

CommandOptions ListOperation::describeOptions() const
{
    CommandOptions options;
    // CommandOptions uses builder pattern, not direct field assignment
    return options;
}

void ListOperation::displayTemplateList()
{
    auto templates = impl_->service_->listAllTemplates();

    if (templates.empty()) {
        if (presenter()) {
            presenter()->displayInfo("No templates available.");
            presenter()->displayInfo("Run 'scrap template update' to download templates.");
        }
        return;
    }

    if (presenter()) {
        presenter()->displayInfo("Available templates:");
        presenter()->displayInfo("");

        for (const auto& tmpl : templates) {
            std::string line = "  " + tmpl.name();
            if (!tmpl.description().empty()) {
                line += " - " + tmpl.description();
            }
            presenter()->displayInfo(line);
        }
    }
}

void ListOperation::displayTemplatesBySource()
{
    auto sources = impl_->service_->listTemplateSources();

    if (sources.empty()) {
        if (presenter()) {
            presenter()->displayInfo("No template sources configured.");
        }
        return;
    }

    bool hasAnyTemplates = false;

    if (presenter()) {
        presenter()->displayInfo("Available templates by source:");
        presenter()->displayInfo("");
    }

    for (const auto& source : sources) {
        auto templates = impl_->service_->listTemplatesFromSource(source.name);

        if (!templates.empty()) {
            hasAnyTemplates = true;

            if (presenter()) {
                presenter()->displayInfo("From " + source.name + ":");

                for (const auto& tmpl : templates) {
                    std::string line = "  " + tmpl.name();
                    if (!tmpl.description().empty()) {
                        line += " - " + tmpl.description();
                    }
                    presenter()->displayInfo(line);
                }
                presenter()->displayInfo("");
            }
        }
    }

    if (!hasAnyTemplates && presenter()) {
        presenter()->displayInfo("No templates available.");
        presenter()->displayInfo("Run 'scrap template update' to download templates.");
    }
}

} // namespace scrap::template_system::command
