#include "ListOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "toolchain/model/Toolchain.h"
#include "shared/presentation/Presenter.h"
#include <sstream>
#include <algorithm>

namespace scrap::toolchain::command {

ListOperation::ListOperation(std::shared_ptr<service::ToolchainService> service)
    : service_(service) {
}

void ListOperation::execute(const std::vector<std::string>& args) {
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    if (!args.empty() && (args[0] == "--help" || args[0] == "-h")) {
        presenter->displayInfo("List all installed toolchains");
        presenter->displayInfo("");
        presenter->displayInfo("Usage: scrap toolchain list");
        return;
    }

    if (!service_) {
        presenter->displayError("Toolchain service not available");
        return;
    }

    // Get all installed toolchains
    auto toolchains = service_->listInstalled();

    if (toolchains.empty()) {
        presenter->displayInfo("No toolchains installed");
        presenter->displayInfo("Run 'scrap toolchain install <toolchain>' to install a toolchain");
        return;
    }

    // Get current toolchain
    auto current = service_->getCurrentToolchain();

    // Display header (rustup-style)
    presenter->displayInfo("installed toolchains");
    presenter->displayInfo("--------------------");

    // Sort toolchains for consistent display
    std::sort(toolchains.begin(), toolchains.end(),
        [](const model::Toolchain& a, const model::Toolchain& b) {
            return a.getTriple() < b.getTriple();
        });

    // Display each toolchain
    for (const auto& toolchain : toolchains) {
        std::stringstream ss;
        ss << "  " << toolchain.getTriple();
        if (current && toolchain.getId() == current->getId()) {
            ss << " (default)";
        }
        presenter->displayInfo(ss.str());
    }

    // Display active toolchain details
    if (current) {
        presenter->displayInfo("");
        presenter->displayInfo("active toolchain");
        presenter->displayInfo("----------------");

        std::stringstream ss;
        ss << current->getTriple() << " (default)";
        presenter->displayInfo(ss.str());

        if (current->getInstallationPath()) {
            ss.str("");
            ss << "  installed: " << current->getInstallationPath()->string();
            presenter->displayInfo(ss.str());
        }

        ss.str("");
        ss << "  version: " << current->getName().toString() << " " << current->getVersion().toString();
        presenter->displayInfo(ss.str());
    }
}

} // namespace scrap::toolchain::command
