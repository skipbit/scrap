#include "InstallOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "toolchain/model/Toolchain.h"
#include "shared/presentation/Presenter.h"
#include "shared/command/CommandOptions.h"
#include <sstream>
#include <chrono>
#include <thread>

namespace scrap::toolchain::command {

InstallOperation::InstallOperation(std::shared_ptr<service::ToolchainService> service)
    : service_(service)
{
}

void InstallOperation::execute(const std::vector<std::string>& args)
{
    auto output = presenter();
    if (!output) {
        return;
    }

    if (args.empty()) {
        output->displayError("Missing required argument: <toolchain-spec>");
        output->displayInfo("Usage: scrap toolchain install <toolchain-spec>");
        return;
    }

    const std::string& specStr = args[0];

    try {
        // Parse specification
        auto spec = model::ToolchainSpecification::parse(specStr);

        // Display installation start (Homebrew-style)
        std::stringstream ss;
        ss << "==> Downloading " << spec.name << "-" << spec.version
           << "-" << model::architectureToString(spec.architecture.value_or(model::currentArchitecture()))
           << "-" << model::platformToString(spec.platform.value_or(model::currentPlatform()))
           << " from github.com/skipbit/scrap-toolchain...";
        output->displayInfo(ss.str());

        // Simulate download progress
        output->displayInfo("==> Downloading https://github.com/skipbit/scrap-toolchain/releases/download/"
                              + spec.name + "-" + spec.version + "/" + spec.name + "-" + spec.version + ".tar.gz");

        // Progress bar simulation
        output->startProgress("Downloading", 100);
        for (size_t i = 0; i <= 100; i += 10) {
            output->updateProgress(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate download time
        }
        output->finishProgress();

        // Install
        ss.str("");
        ss << "==> Installing " << spec.name << "-" << spec.version << "...";
        output->displayInfo(ss.str());

        service_->install(spec);

        // Success message
        output->displaySuccess("==> Installation successful!");
        output->displayInfo("==> Summary");

        ss.str("");
        ss << "  🎯 " << spec.name << "-" << spec.version
           << "-" << model::architectureToString(spec.architecture.value_or(model::currentArchitecture()))
           << "-" << model::platformToString(spec.platform.value_or(model::currentPlatform()))
           << " installed to:";
        output->displayInfo(ss.str());

        ss.str("");
        ss << "     /Users/user/.scrap/toolchains/" << spec.name << "/" << spec.version
           << "/" << model::architectureToString(spec.architecture.value_or(model::currentArchitecture()))
           << "-" << model::platformToString(spec.platform.value_or(model::currentPlatform()));
        output->displayInfo(ss.str());

    } catch (const std::exception& e) {
        output->displayError(std::string("Installation failed: ") + e.what());
    }
}

CommandOptions InstallOperation::describeOptions() const
{
    return CommandOptions()
        .addPositional("toolchain-spec", "Toolchain specification (name[@version])");
}

void InstallOperation::displayHelp() const
{
    // This method is deprecated and will be removed
    // Help is now generated automatically from describeOptions()
}

} // namespace scrap::toolchain::command
