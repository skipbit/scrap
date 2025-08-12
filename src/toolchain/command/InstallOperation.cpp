#include "InstallOperation.h"
#include "toolchain/service/ToolchainService.h"
#include "toolchain/model/Toolchain.h"
#include "shared/presentation/Presenter.h"
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

    if (args.empty() || args[0] == "--help") {
        displayHelp();
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

void InstallOperation::displayHelp() const
{
    auto output = presenter();
    if (!output) {
        return;
    }

    output->displayInfo("Install a new toolchain");
    output->displayInfo("");
    output->displayInfo("Usage: scrap toolchain install <toolchain-spec>");
    output->displayInfo("");
    output->displayInfo("Arguments:");
    output->displayInfo("  <toolchain-spec>  Toolchain specification (name[@version])");
    output->displayInfo("");
    output->displayInfo("Examples:");
    output->displayInfo("  scrap toolchain install llvm           # Install latest LLVM");
    output->displayInfo("  scrap toolchain install llvm@19.0.0    # Install specific version");
    output->displayInfo("  scrap toolchain install gcc@13.2.0     # Install GCC 13.2.0");
}

} // namespace scrap::toolchain::command
