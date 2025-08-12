#include "SelectOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "toolchain/model/Toolchain.h"
#include "shared/presentation/Presenter.h"
#include <sstream>

namespace scrap::toolchain::command {

SelectOperation::SelectOperation(std::shared_ptr<service::ToolchainService> service)
    : service_(service)
{
}

void SelectOperation::execute(const std::vector<std::string>& args)
{
    auto output = presenter();
    if (!output) {
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
        output->displayError("Toolchain not found: " + toolchainId);
        output->displayInfo("Run 'scrap toolchain list' to see available toolchains");
        return;
    }

    // Display selection info (cargo-style)
    std::stringstream ss;
    ss << "info: using existing install for '" << toolchain->triple() << "'";
    output->displayInfo(ss.str());

    // Select the toolchain
    auto result = service_->select(toolchain->id());
    if (!result) {
        output->displayError("Selection failed: " + result.error());
        return;
    }

    ss.str("");
    ss << "info: default toolchain set to '" << toolchain->triple() << "'";
    output->displayInfo(ss.str());

    // Display selected toolchain info
    output->displayInfo("");
    ss.str("");
    ss << "  " << toolchain->triple() << " (default)";
    output->displayInfo(ss.str());

    ss.str("");
    ss << "  " << toolchain->name().toString() << " version " << toolchain->version().toString();
    output->displayInfo(ss.str());
}

void SelectOperation::displayHelp() const
{
    auto output = presenter();
    if (!output) {
        return;
    }

    output->displayInfo("Select a toolchain as the default");
    output->displayInfo("");
    output->displayInfo("Usage: scrap toolchain select <toolchain-id>");
    output->displayInfo("");
    output->displayInfo("Arguments:");
    output->displayInfo("  <toolchain-id>  Full toolchain identifier");
    output->displayInfo("");
    output->displayInfo("Examples:");
    output->displayInfo("  scrap toolchain select llvm-18.0.0-x86_64-darwin");
    output->displayInfo("  scrap toolchain select gcc-13.2.0");
    output->displayInfo("");
    output->displayInfo("Note: Run 'scrap toolchain list' to see available toolchains");
}

} // namespace scrap::toolchain::command
