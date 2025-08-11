#include "TemplateModule.h"

namespace scrap::template_system {

std::shared_ptr<service::TemplateService> TemplateModule::createTemplateService() {
    return std::make_shared<service::DefaultTemplateService>();
}

std::shared_ptr<service::TemplateService> TemplateModule::createTemplateService(
    const std::filesystem::path& templatesDir) {
    return std::make_shared<service::DefaultTemplateService>(templatesDir);
}

} // namespace scrap::template_system
