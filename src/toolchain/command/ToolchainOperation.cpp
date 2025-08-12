#include "ToolchainOperation.h"
#include "ListOperation.h"
#include "InstallOperation.h"
#include "SelectOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "shared/presentation/Presenter.h"

namespace scrap::toolchain::command {

ToolchainOperation::ToolchainOperation(std::shared_ptr<service::ToolchainService> service)
    : service_(service)
{
    // Register subcommands
    addSubOperation("list", std::make_shared<ListOperation>(service));
    addSubOperation("install", std::make_shared<InstallOperation>(service));
    addSubOperation("select", std::make_shared<SelectOperation>(service));
}

void ToolchainOperation::displayHelp() const
{
    auto output = presenter();
    if (!output) {
        return;
    }

    output->displayInfo("Manage toolchains for building C++ projects");
    output->displayInfo("");
    output->displayInfo("Usage: scrap toolchain <subcommand> [options]");
    output->displayInfo("");
    output->displayInfo("Available subcommands:");
    output->displayInfo("  list      List all installed toolchains");
    output->displayInfo("  install   Install a new toolchain");
    output->displayInfo("  select    Select a toolchain as the default");
    output->displayInfo("");
    output->displayInfo("Examples:");
    output->displayInfo("  scrap toolchain list");
    output->displayInfo("  scrap toolchain install llvm@19.0.0");
    output->displayInfo("  scrap toolchain select gcc-13.2.0");
}

} // namespace scrap::toolchain::command
