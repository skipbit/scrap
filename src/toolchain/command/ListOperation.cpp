#include "ListOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "toolchain/model/Toolchain.h"
#include "shared/presentation/Presenter.h"
#include "shared/command/CommandOptions.h"
#include <sstream>
#include <algorithm>

namespace scrap::toolchain::command {

ListOperation::ListOperation(std::shared_ptr<service::ToolchainService> service)
    : service_(service)
{
}

void ListOperation::execute(const std::vector<std::string>& /* args */)
{
    auto output = presenter();
    if (!output) {
        return;
    }

    // Note: Help is now handled by CLI11, no need to check for --help here

    if (!service_) {
        output->displayError("Toolchain service not available");
        return;
    }

    // Get all installed toolchains
    auto toolchains = service_->listInstalled();

    if (toolchains.empty()) {
        output->displayInfo("No toolchains installed");
        output->displayInfo("Run 'scrap toolchain install <toolchain>' to install a toolchain");
        return;
    }

    // Get current toolchain
    auto current = service_->currentToolchain();

    // Display header (rustup-style)
    output->displayInfo("installed toolchains");
    output->displayInfo("--------------------");

    // Sort toolchains for consistent display
    std::sort(toolchains.begin(), toolchains.end(),
        [](const model::Toolchain& a, const model::Toolchain& b) {
            return a.triple() < b.triple();
        });

    // Display each toolchain
    for (const auto& toolchain : toolchains) {
        std::stringstream ss;
        ss << "  " << toolchain.triple();
        if (current && toolchain.id() == current->id()) {
            ss << " (default)";
        }
        output->displayInfo(ss.str());
    }

    // Display active toolchain details
    if (current) {
        output->displayInfo("");
        output->displayInfo("active toolchain");
        output->displayInfo("----------------");

        std::stringstream ss;
        ss << current->triple() << " (default)";
        output->displayInfo(ss.str());

        if (current->installationPath()) {
            ss.str("");
            ss << "  installed: " << current->installationPath()->string();
            output->displayInfo(ss.str());
        }

        ss.str("");
        ss << "  version: " << current->name().toString() << " " << current->version().toString();
        output->displayInfo(ss.str());
    }
}

CommandOptions ListOperation::describeOptions() const
{
    // No options for list command
    return CommandOptions();
}

} // namespace scrap::toolchain::command
