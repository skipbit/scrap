#include "shared/command/Application.h"
#include "shared/command/ApplicationCommandHandler.h"
#include <memory>

namespace scrap {

Application::Application()
    : commandHandler_(ApplicationCommandHandlerFactory::create())
{
}

Application::~Application() = default;

Application::Application(Application&&) noexcept = default;

Application& Application::operator=(Application&&) noexcept = default;

int Application::run(int argc, const char* const argv[])
{
    commandHandler_->configureCommands();
    return commandHandler_->execute(argc, argv);
}


}
