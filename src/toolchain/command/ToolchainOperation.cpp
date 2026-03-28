#include "ToolchainOperation.h"
#include "InstallOperation.h"
#include "ListOperation.h"
#include "SelectOperation.h"
#include "shared/command/CommandOptions.h"
#include "shared/presentation/Presenter.h"
#include "toolchain/service/ToolchainService.h"

namespace scrap::toolchain::command {

ToolchainOperation::ToolchainOperation(std::shared_ptr<service::ToolchainService> service)
    : service_(service)
{
    // Register subcommands
    addSubOperation("list", std::make_shared<ListOperation>(service));
    addSubOperation("install", std::make_shared<InstallOperation>(service));
    addSubOperation("select", std::make_shared<SelectOperation>(service));
}

CommandOptions ToolchainOperation::describeOptions() const
{
    // No options for the parent toolchain command itself
    return CommandOptions();
}

void ToolchainOperation::displayHelp() const
{
    // This method is deprecated and will be removed
    // Help is now generated automatically from describeOptions()
    // Subcommands are displayed automatically by CLI11
}

}  // namespace scrap::toolchain::command
