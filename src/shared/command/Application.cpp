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

int Application::run(std::span<const char* const> args)
{
    commandHandler_->configureCommands();
    return commandHandler_->execute(args);
}

}  // namespace scrap
