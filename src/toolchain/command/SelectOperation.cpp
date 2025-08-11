#include "SelectOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "toolchain/model/Toolchain.h"
#include "shared/presentation/Presenter.h"
#include <sstream>

namespace scrap::toolchain::command {

SelectOperation::SelectOperation(std::shared_ptr<service::ToolchainService> service)
    : service_(service) {
}

void SelectOperation::execute(const std::vector<std::string>& args) {
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    if (args.empty() || args[0] == "--help") {
        displayHelp();
        return;
    }

    const std::string& toolchainId = args[0];

    // Find the toolchain
    auto toolchain = service_->findById(model::ToolchainId(toolchainId));
    if (!toolchain) {
        presenter->displayError("Toolchain not found: " + toolchainId);
        presenter->displayInfo("Run 'scrap toolchain list' to see available toolchains");
        return;
    }

    // Display selection info (cargo-style)
    std::stringstream ss;
    ss << "info: using existing install for '" << toolchain->triple() << "'";
    presenter->displayInfo(ss.str());

    // Select the toolchain
    auto result = service_->select(toolchain->id());
    if (!result) {
        presenter->displayError("Selection failed: " + result.error());
        return;
    }

    ss.str("");
    ss << "info: default toolchain set to '" << toolchain->triple() << "'";
    presenter->displayInfo(ss.str());

    // Display selected toolchain info
    presenter->displayInfo("");
    ss.str("");
    ss << "  " << toolchain->triple() << " (default)";
    presenter->displayInfo(ss.str());

    ss.str("");
    ss << "  " << toolchain->name().toString() << " version " << toolchain->version().toString();
    presenter->displayInfo(ss.str());
}

void SelectOperation::displayHelp() const {
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    presenter->displayInfo("Select a toolchain as the default");
    presenter->displayInfo("");
    presenter->displayInfo("Usage: scrap toolchain select <toolchain-id>");
    presenter->displayInfo("");
    presenter->displayInfo("Arguments:");
    presenter->displayInfo("  <toolchain-id>  Full toolchain identifier");
    presenter->displayInfo("");
    presenter->displayInfo("Examples:");
    presenter->displayInfo("  scrap toolchain select llvm-18.0.0-x86_64-darwin");
    presenter->displayInfo("  scrap toolchain select gcc-13.2.0");
    presenter->displayInfo("");
    presenter->displayInfo("Note: Run 'scrap toolchain list' to see available toolchains");
}

} // namespace scrap::toolchain::command
