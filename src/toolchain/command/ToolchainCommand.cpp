#include "toolchain/command/ToolchainCommand.h"
#include "toolchain/command/ListCommand.h"
#include "shared/command/Command.h"

#include <iostream>

namespace scrap::toolchain {

ToolchainCommand::ToolchainCommand()
{
}

ToolchainCommand::~ToolchainCommand()
{
}

void ToolchainCommand::setup(Command& cmd)
{
    cmd.add("list", makeCommand<ListCommand>());
}

void ToolchainCommand::execute(const std::vector<std::string>& /*args*/)
{
    std::cout << "Usage: scrap toolchain <subcommand>" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Available subcommands:" << std::endl;
    std::cout << "  list    List installed toolchains" << std::endl;
}

}
