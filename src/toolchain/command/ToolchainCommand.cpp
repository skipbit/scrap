#include "toolchain/command/ToolchainCommand.h"
#include "toolchain/command/ListCommand.h"

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

void ToolchainCommand::execute(const std::vector<Command::Option>&)
{
    std::cerr << "toolchain help implementation" << std::endl;
}

}
