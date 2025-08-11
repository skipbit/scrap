#include "toolchain/ToolchainModule.h"
#include "toolchain/command/ToolchainOperation.h"
#include "toolchain/command/ListOperation.h"
#include "toolchain/command/InstallOperation.h"
#include "toolchain/command/SelectOperation.h"
#include "toolchain/service/ToolchainService.h"
#include <memory>

namespace scrap::toolchain {

void ToolchainModule::registerCommands(CommandDispatcher& dispatcher,
                                       std::shared_ptr<CLIParser> /* parser */,
                                       std::shared_ptr<Presenter> presenter)
{
    // Create Mock service for now
    auto service = std::make_shared<service::MockToolchainService>();

    // Create individual operations
    auto listOp = std::make_shared<command::ListOperation>(service);
    listOp->setPresenter(presenter);

    auto installOp = std::make_shared<command::InstallOperation>(service);
    installOp->setPresenter(presenter);

    auto selectOp = std::make_shared<command::SelectOperation>(service);
    selectOp->setPresenter(presenter);

    // Create main toolchain operation for help
    auto toolchainOp = std::make_shared<command::ToolchainOperation>(service);
    toolchainOp->setPresenter(presenter);

    // Register operations with hierarchical names
    dispatcher.registerOperation("toolchain", toolchainOp);
    dispatcher.registerOperation("toolchain.list", listOp);
    dispatcher.registerOperation("toolchain.install", installOp);
    dispatcher.registerOperation("toolchain.select", selectOp);
}

std::vector<std::pair<std::string, std::string>> ToolchainModule::getAvailableCommands()
{
    return {
        {"toolchain", "Manages the toolchain for the project"}
    };
}

std::vector<std::pair<std::string, std::string>> ToolchainModule::getAvailableSubcommands()
{
    return {
        {"list", "Display installed toolchains and indicate which one is currently selected"},
        {"install", "Install a new toolchain from the ecosystem"},
        {"select", "Select a toolchain as the default"}
    };
}

} // namespace scrap::toolchain
