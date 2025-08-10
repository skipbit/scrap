#include "shared/command/Application.h"
#include "shared/command/Command.h"
#include "toolchain/command/ToolchainCommand.h"

#include <iostream>
#include <cstdlib>

namespace scrap {

Application::Application() = default;

Application::~Application() = default;

void Application::setup(Command& cmd)
{
    cmd.add("toolchain", makeCommand<toolchain::ToolchainCommand>());
}

void Application::execute(const std::vector<std::string>& /*args*/)
{
    std::cout << "scrap - Modern C++ development tool" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Usage: scrap <subcommand> [options]" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Available subcommands:" << std::endl;
    std::cout << "  toolchain    Manage toolchains" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Use 'scrap <subcommand> --help' for more information about a subcommand." << std::endl;
}

void Application::run(int argc, const char* const argv[])
{
    // Create makeCommand and use Command::run
    auto app = makeCommand<Application>();
    app.run(argc, argv);  // Delegate to Command::run
}

}
