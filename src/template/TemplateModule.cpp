#include "TemplateModule.h"

namespace scrap::template_system {

std::shared_ptr<service::TemplateService> TemplateModule::createTemplateService(
    std::shared_ptr<Presenter> presenter)
{
    return std::make_shared<service::DefaultTemplateService>(
        service::DefaultTemplateService::getDefaultTemplatesDirectory(),
        nullptr,
        presenter);
}

std::shared_ptr<service::TemplateService> TemplateModule::createTemplateService(
    const std::filesystem::path& templatesDir,
    std::shared_ptr<Presenter> presenter)
{
    return std::make_shared<service::DefaultTemplateService>(templatesDir, nullptr, presenter);
}

} // namespace scrap::template_system
