#include "toolchain/command/ListOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "shared/presentation/Presenter.h"

namespace scrap::toolchain {

ListOperation::ListOperation(std::shared_ptr<ToolchainService> service)
    : service_(service)
{
}

ListOperation::~ListOperation() = default;

void ListOperation::execute(const std::vector<std::string>& /*args*/)
{
    auto presenter = getPresenter();
    if (!presenter) {
        return; // No presenter available
    }
    
    if (!service_) {
        presenter->showError("Toolchain service not available");
        return;
    }
    
    // Ensure registry is up-to-date
    if (!service_->ensureRegistryUpToDate()) {
        presenter->showError("Failed to update toolchain registry");
        return;
    }
    
    // Get all available toolchains
    auto toolchains = service_->getAllToolchains();
    
    if (toolchains.empty()) {
        presenter->showInfo("No toolchains available");
        return;
    }
    
    // Format toolchain information
    std::vector<std::string> toolchainList;
    auto defaultToolchain = service_->getDefaultToolchain();
    std::string defaultName = defaultToolchain ? defaultToolchain->getFullIdentifier() : "";
    
    for (const auto& toolchain : toolchains) {
        std::string entry = toolchain.getFullIdentifier();
        if (!defaultName.empty() && toolchain.getFullIdentifier() == defaultName) {
            entry += " (default)";
        }
        toolchainList.push_back(entry);
    }
    
    // Display the list
    presenter->showList("Installed toolchains", toolchainList);
    
    if (defaultName.empty()) {
        presenter->showInfo("No default toolchain selected");
    }
}

}