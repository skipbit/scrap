#include "shared/command/Application.h"
#include "toolchain/command/ToolchainCommand.h"

#include <iostream>

namespace scrap {

Application::Application() = default;

Application::~Application() = default;

void Application::setup(Command& cmd)
{
    cmd.add("toolchain", makeCommand<toolchain::ToolchainCommand>());
}

void Application::execute(const std::vector<Command::Option>&)
{
    std::cerr << "application help implementation" << std::endl;
}

}
