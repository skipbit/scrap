#include "shared/command/ApplicationCommandHandler.h"
#include "shared/command/Operation.h"
#include "shared/command/CommandOptions.h"
#include "shared/command/driver/CLI11Parser.h"
#include "shared/command/driver/CLI11CommandDispatcher.h"
#include "shared/presentation/driver/ConsolePresenter.h"
#include "toolchain/ToolchainModule.h"
#include "project/ProjectModule.h"
#include "project/command/NewOperation.h"
#include "project/command/BuildOperation.h"
#include "project/command/RunOperation.h"
#include "project/command/CleanOperation.h"
#include "project/service/ProjectService.h"
#include "toolchain/command/ListOperation.h"
#include "toolchain/command/InstallOperation.h"
#include "toolchain/command/SelectOperation.h"
#include "toolchain/service/ToolchainService.h"
#include <CLI/CLI.hpp>
#include <iostream>

namespace scrap {

ApplicationCommandHandler::ApplicationCommandHandler(std::unique_ptr<CLIParser> parser,
                                                     std::unique_ptr<CommandDispatcher> dispatcher,
                                                     std::shared_ptr<Presenter> presenter)
    : parser_(std::move(parser)), dispatcher_(std::move(dispatcher)), presenter_(presenter)
{
    // Set presenter on parser if it's a CLI11Parser
    if (auto* cli11Parser = dynamic_cast<CLI11Parser*>(parser_.get())) {
        cli11Parser->setPresenter(presenter_);
    }
}

ApplicationCommandHandler::~ApplicationCommandHandler() = default;

ApplicationCommandHandler::ApplicationCommandHandler(ApplicationCommandHandler&&) noexcept = default;

ApplicationCommandHandler& ApplicationCommandHandler::operator=(ApplicationCommandHandler&&) noexcept = default;

int ApplicationCommandHandler::execute(int argc, const char* const argv[])
{
    try {
        // Parse command line arguments
        CommandRequest request = parser_->parse(argc, argv);

        // Dispatch to appropriate command
        CommandResult result = dispatcher_->dispatch(request);

        // Handle result
        switch (result.status()) {
            case CommandResult::Status::Success:
                if (!result.message().empty()) {
                    std::cout << result.message() << std::endl;
                }
                return 0;

            case CommandResult::Status::Failure:
                std::cerr << "Error: " << result.message() << std::endl;
                return 1;

            case CommandResult::Status::InvalidCommand:
                std::cerr << result.message() << std::endl;
                return 1;
        }

    } catch (const CLI::ParseError& e) {
        // Handle CLI11 errors properly - note this is a simplified version
        // In the actual CLI11 integration, we would need access to the CLI::App to properly handle this
        std::cerr << "Usage error: " << e.what() << std::endl;
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

void ApplicationCommandHandler::registerRootOperation(const std::string& commandName,
                                                      std::shared_ptr<Operation> operation)
{
    dispatcher_->registerOperation(commandName, operation);
}

void ApplicationCommandHandler::configureCommands()
{
    registerDomainModules();
    setupCommandStructure();
}

void ApplicationCommandHandler::registerDomainModules()
{
    // Register toolchain domain module
    toolchain::ToolchainModule::registerCommands(*dispatcher_,
                                                  std::shared_ptr<CLIParser>(parser_.get(), [](CLIParser*){}),
                                                  presenter_);

    // Register project domain module
    project::ProjectModule::registerCommands(*dispatcher_,
                                              std::shared_ptr<CLIParser>(parser_.get(), [](CLIParser*){}),
                                              presenter_);

    // TODO: Register other domain modules as they are implemented
    // package::PackageModule::registerCommands(*dispatcher_, parser_, presenter_);
}

void ApplicationCommandHandler::setupCommandStructure()
{
    // Get available commands from domain modules
    auto toolchainCommands = toolchain::ToolchainModule::availableCommands();
    auto projectCommands = project::ProjectModule::availableCommands();

    // Merge commands
    std::vector<std::pair<std::string, std::string>> allCommands;
    allCommands.insert(allCommands.end(), toolchainCommands.begin(), toolchainCommands.end());
    allCommands.insert(allCommands.end(), projectCommands.begin(), projectCommands.end());

    // Configure root commands
    parser_->configureCommands(allCommands);

    // Configure subcommands
    auto toolchainSubcommands = toolchain::ToolchainModule::availableSubcommands();
    parser_->configureSubcommands("toolchain", toolchainSubcommands);

    // Now configure command options after commands are set up
    configureCommandOptions();
}

void ApplicationCommandHandler::configureCommandOptions()
{
    // Configure options for all project commands
    auto projectService = std::make_shared<project::service::MockProjectService>(nullptr, presenter_);

    // Project commands
    auto newOp = std::make_shared<project::command::NewOperation>(projectService);
    parser_->configureCommandOptions("new", newOp->describeOptions());

    auto buildOp = std::make_shared<project::command::BuildOperation>(projectService);
    parser_->configureCommandOptions("build", buildOp->describeOptions());

    auto runOp = std::make_shared<project::command::RunOperation>(projectService);
    parser_->configureCommandOptions("run", runOp->describeOptions());

    auto cleanOp = std::make_shared<project::command::CleanOperation>(projectService);
    parser_->configureCommandOptions("clean", cleanOp->describeOptions());

    // Configure options for toolchain subcommands
    auto toolchainService = std::make_shared<toolchain::service::MockToolchainService>();

    auto listOp = std::make_shared<toolchain::command::ListOperation>(toolchainService);
    parser_->configureCommandOptions("toolchain.list", listOp->describeOptions());

    auto installOp = std::make_shared<toolchain::command::InstallOperation>(toolchainService);
    parser_->configureCommandOptions("toolchain.install", installOp->describeOptions());

    auto selectOp = std::make_shared<toolchain::command::SelectOperation>(toolchainService);
    parser_->configureCommandOptions("toolchain.select", selectOp->describeOptions());
}

// ApplicationCommandHandlerFactory implementation
std::unique_ptr<ApplicationCommandHandler> ApplicationCommandHandlerFactory::create()
{
    auto parserFactory = std::make_unique<CLI11ParserFactory>();
    auto parser = parserFactory->createParser("scrap", "Modern C++ development tool");
    auto dispatcher = std::make_unique<CLI11CommandDispatcher>();
    auto presenterFactory = std::make_unique<ConsolePresenterFactory>();
    auto presenter = presenterFactory->createPresenter();

    return std::make_unique<ApplicationCommandHandler>(std::move(parser), std::move(dispatcher), std::move(presenter));
}

std::unique_ptr<ApplicationCommandHandler> ApplicationCommandHandlerFactory::create(
    std::unique_ptr<CLIParserFactory> parserFactory,
    std::unique_ptr<CommandDispatcher> dispatcher)
{
    auto parser = parserFactory->createParser("scrap", "Modern C++ development tool");
    auto presenterFactory = std::make_unique<ConsolePresenterFactory>();
    auto presenter = presenterFactory->createPresenter();

    return std::make_unique<ApplicationCommandHandler>(std::move(parser), std::move(dispatcher), std::move(presenter));
}

}
