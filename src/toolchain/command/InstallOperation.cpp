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
    auto presenter = getPresenter();
    if (!presenter) {
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
           << "-" << model::architectureToString(spec.architecture.value_or(model::getCurrentArchitecture()))
           << "-" << model::platformToString(spec.platform.value_or(model::getCurrentPlatform()))
           << " from github.com/skipbit/scrap-toolchain...";
        presenter->displayInfo(ss.str());

        // Simulate download progress
        presenter->displayInfo("==> Downloading https://github.com/skipbit/scrap-toolchain/releases/download/"
                              + spec.name + "-" + spec.version + "/" + spec.name + "-" + spec.version + ".tar.gz");

        // Progress bar simulation
        presenter->startProgress("Downloading", 100);
        for (size_t i = 0; i <= 100; i += 10) {
            presenter->updateProgress(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate download time
        }
        presenter->finishProgress();

        // Install
        ss.str("");
        ss << "==> Installing " << spec.name << "-" << spec.version << "...";
        presenter->displayInfo(ss.str());

        service_->install(spec);

        // Success message
        presenter->displaySuccess("==> Installation successful!");
        presenter->displayInfo("==> Summary");

        ss.str("");
        ss << "  🎯 " << spec.name << "-" << spec.version
           << "-" << model::architectureToString(spec.architecture.value_or(model::getCurrentArchitecture()))
           << "-" << model::platformToString(spec.platform.value_or(model::getCurrentPlatform()))
           << " installed to:";
        presenter->displayInfo(ss.str());

        ss.str("");
        ss << "     /Users/user/.scrap/toolchains/" << spec.name << "/" << spec.version
           << "/" << model::architectureToString(spec.architecture.value_or(model::getCurrentArchitecture()))
           << "-" << model::platformToString(spec.platform.value_or(model::getCurrentPlatform()));
        presenter->displayInfo(ss.str());

    } catch (const std::exception& e) {
        presenter->displayError(std::string("Installation failed: ") + e.what());
    }
}

void InstallOperation::displayHelp() const
{
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    presenter->displayInfo("Install a new toolchain");
    presenter->displayInfo("");
    presenter->displayInfo("Usage: scrap toolchain install <toolchain-spec>");
    presenter->displayInfo("");
    presenter->displayInfo("Arguments:");
    presenter->displayInfo("  <toolchain-spec>  Toolchain specification (name[@version])");
    presenter->displayInfo("");
    presenter->displayInfo("Examples:");
    presenter->displayInfo("  scrap toolchain install llvm           # Install latest LLVM");
    presenter->displayInfo("  scrap toolchain install llvm@19.0.0    # Install specific version");
    presenter->displayInfo("  scrap toolchain install gcc@13.2.0     # Install GCC 13.2.0");
}

} // namespace scrap::toolchain::command
