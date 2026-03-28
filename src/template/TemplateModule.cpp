#include "TemplateModule.h"
#include "shared/command/CLIParser.h"
#include "shared/command/CommandDispatcher.h"
#include "shared/presentation/Presenter.h"
#include "template/command/TemplateOperation.h"

namespace scrap::template_system {

std::shared_ptr<service::TemplateService> TemplateModule::createTemplateService(std::shared_ptr<Presenter> presenter)
{
    // Get default templates directory, fallback to ./scrap/templates if it fails
    auto templatesDir = service::DefaultTemplateService::defaultTemplatesDirectory();
    std::filesystem::path path = templatesDir.has_value() ? *templatesDir : std::filesystem::path("./scrap/templates");

    return std::make_shared<service::DefaultTemplateService>(path, nullptr, presenter);
}

std::shared_ptr<service::TemplateService>
TemplateModule::createTemplateService(const std::filesystem::path& templatesDir, std::shared_ptr<Presenter> presenter)
{
    return std::make_shared<service::DefaultTemplateService>(templatesDir, nullptr, presenter);
}

void TemplateModule::registerCommands(CommandDispatcher& dispatcher,
                                      std::shared_ptr<CLIParser> /* parser */,
                                      std::shared_ptr<Presenter> presenter)
{
    auto templateService = createTemplateService(presenter);
    auto templateOperation = std::make_shared<command::TemplateOperation>(templateService);
    templateOperation->setPresenter(presenter);

    dispatcher.registerOperation("template", templateOperation);
}

std::vector<std::pair<std::string, std::string>> TemplateModule::availableCommands()
{
    return {{"template", "Manage project templates"}};
}

std::vector<std::pair<std::string, std::string>> TemplateModule::availableSubcommands()
{
    return {{"list", "List available templates"}, {"update", "Update template sources"}};
}

}  // namespace scrap::template_system
