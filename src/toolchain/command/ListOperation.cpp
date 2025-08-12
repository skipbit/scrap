#include "ListOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "toolchain/model/Toolchain.h"
#include "shared/presentation/Presenter.h"
#include <sstream>
#include <algorithm>

namespace scrap::toolchain::command {

ListOperation::ListOperation(std::shared_ptr<service::ToolchainService> service)
    : service_(service)
{
}

void ListOperation::execute(const std::vector<std::string>& args)
{
    auto output = presenter();
    if (!output) {
        return;
    }

    if (!args.empty() && (args[0] == "--help" || args[0] == "-h")) {
        output->displayInfo("List all installed toolchains");
        output->displayInfo("");
        output->displayInfo("Usage: scrap toolchain list");
        return;
    }

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

} // namespace scrap::toolchain::command
