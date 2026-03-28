#include "SelectOperation.h"
#include "shared/command/CommandOptions.h"
#include "shared/presentation/Presenter.h"
#include "toolchain/model/Toolchain.h"
#include "toolchain/service/ToolchainService.h"
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

    if (args.empty()) {
        output->displayError("Missing required argument: <toolchain-id>");
        output->displayInfo("Usage: scrap toolchain select <toolchain-id>");
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

CommandOptions SelectOperation::describeOptions() const
{
    return CommandOptions().addPositional("toolchain-id", "Full toolchain identifier");
}

void SelectOperation::displayHelp() const
{
    // This method is deprecated and will be removed
    // Help is now generated automatically from describeOptions()
}

}  // namespace scrap::toolchain::command
