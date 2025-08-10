#include "toolchain/ToolchainModule.h"
#include "toolchain/command/ListOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "toolchain/driver/ToolchainRepository.h"
#include "shared/command/HelpCommand.h"
#include "shared/command/CLIParser.h"
#include <memory>

namespace scrap::toolchain {

void ToolchainModule::registerCommands(CommandDispatcher& dispatcher, 
                                       std::shared_ptr<CLIParser> parser,
                                       std::shared_ptr<Presenter> presenter)
{
    // Create infrastructure dependencies
    auto repository = ToolchainRepositoryFactory::createRepository();
    auto service = std::make_shared<ToolchainService>(repository);
    
    // Create and configure list operation
    auto listOperation = std::make_shared<ListOperation>(service);
    listOperation->setPresenter(presenter);
    
    // Create help command for toolchain
    auto helpCommand = std::make_shared<HelpCommand>(parser, "toolchain");
    helpCommand->setPresenter(presenter);
    
    // Register operations
    dispatcher.registerOperation("toolchain", helpCommand);
    dispatcher.registerOperation("toolchain.list", listOperation);
    
    // TODO: Add other toolchain operations as they are implemented
    // dispatcher.registerOperation("toolchain.install", installOperation);
    // dispatcher.registerOperation("toolchain.build", buildOperation);
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
        {"list", "Display installed toolchains and indicate which one is currently selected"}
        // TODO: Add other subcommands as they are implemented
        // {"install", "Install a new toolchain from the ecosystem"},
        // {"build", "Build a toolchain locally from source"}
    };
}

}