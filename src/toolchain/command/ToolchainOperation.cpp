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
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    presenter->displayInfo("Manage toolchains for building C++ projects");
    presenter->displayInfo("");
    presenter->displayInfo("Usage: scrap toolchain <subcommand> [options]");
    presenter->displayInfo("");
    presenter->displayInfo("Available subcommands:");
    presenter->displayInfo("  list      List all installed toolchains");
    presenter->displayInfo("  install   Install a new toolchain");
    presenter->displayInfo("  select    Select a toolchain as the default");
    presenter->displayInfo("");
    presenter->displayInfo("Examples:");
    presenter->displayInfo("  scrap toolchain list");
    presenter->displayInfo("  scrap toolchain install llvm@19.0.0");
    presenter->displayInfo("  scrap toolchain select gcc-13.2.0");
}

} // namespace scrap::toolchain::command
