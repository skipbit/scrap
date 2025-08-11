#include "ToolchainService.h"
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace scrap::toolchain::service {

using namespace model;

MockToolchainService::MockToolchainService() {
    initializeMockData();
}

void MockToolchainService::initializeMockData() {
    // Create mock toolchains
    auto llvm18 = Toolchain(
        ToolchainId("llvm-18.0.0-x86_64-darwin"),
        ToolchainName("llvm"),
        Version("18.0.0"),
        Architecture::X86_64,
        Platform::Darwin
    );
    llvm18.setInstallationPath("/Users/user/.scrap/toolchains/llvm/18.0.0/x86_64-darwin");
    llvm18.setSelected(true);
    toolchains_.push_back(llvm18);

    auto llvm17 = Toolchain(
        ToolchainId("llvm-17.0.6-x86_64-darwin"),
        ToolchainName("llvm"),
        Version("17.0.6"),
        Architecture::X86_64,
        Platform::Darwin
    );
    llvm17.setInstallationPath("/Users/user/.scrap/toolchains/llvm/17.0.6/x86_64-darwin");
    toolchains_.push_back(llvm17);

    auto gcc13 = Toolchain(
        ToolchainId("gcc-13.2.0-x86_64-darwin"),
        ToolchainName("gcc"),
        Version("13.2.0"),
        Architecture::X86_64,
        Platform::Darwin
    );
    gcc13.setInstallationPath("/Users/user/.scrap/toolchains/gcc/13.2.0/x86_64-darwin");
    toolchains_.push_back(gcc13);

    // Set current toolchain
    currentToolchainId_ = ToolchainId("llvm-18.0.0-x86_64-darwin");
}

std::vector<Toolchain> MockToolchainService::listInstalled() {
    return toolchains_;
}

std::optional<Toolchain> MockToolchainService::getCurrentToolchain() {
    if (!currentToolchainId_) {
        return std::nullopt;
    }
    return findById(*currentToolchainId_);
}

std::optional<Toolchain> MockToolchainService::findById(const ToolchainId& id) {
    auto it = std::find_if(toolchains_.begin(), toolchains_.end(),
        [&id](const Toolchain& t) { return t.getId() == id; });

    if (it != toolchains_.end()) {
        return *it;
    }
    return std::nullopt;
}

void MockToolchainService::install(const ToolchainSpecification& spec) {
    // Check if already installed
    std::string id = spec.name + "-" + spec.version + "-" +
                     architectureToString(spec.architecture.value_or(getCurrentArchitecture())) + "-" +
                     platformToString(spec.platform.value_or(getCurrentPlatform()));

    if (findById(ToolchainId(id))) {
        throw std::runtime_error("Toolchain " + id + " is already installed");
    }

    // Simulate installation
    auto toolchain = Toolchain(
        ToolchainId(id),
        ToolchainName(spec.name),
        Version(spec.version),
        spec.architecture.value_or(getCurrentArchitecture()),
        spec.platform.value_or(getCurrentPlatform())
    );

    std::stringstream pathStream;
    pathStream << "/Users/user/.scrap/toolchains/"
               << spec.name << "/" << spec.version << "/"
               << architectureToString(toolchain.getArchitecture()) << "-"
               << platformToString(toolchain.getPlatform());
    toolchain.setInstallationPath(pathStream.str());

    toolchains_.push_back(toolchain);
}

void MockToolchainService::select(const ToolchainId& id) {
    auto toolchain = findById(id);
    if (!toolchain) {
        throw std::runtime_error("Toolchain not found: " + id.value());
    }

    if (!toolchain->isInstalled()) {
        throw std::runtime_error("Toolchain is not installed: " + id.value());
    }

    // Update selection status
    for (auto& t : toolchains_) {
        t.setSelected(t.getId() == id);
    }
    currentToolchainId_ = id;
}

void MockToolchainService::remove(const ToolchainId& id) {
    auto it = std::find_if(toolchains_.begin(), toolchains_.end(),
        [&id](const Toolchain& t) { return t.getId() == id; });

    if (it == toolchains_.end()) {
        throw std::runtime_error("Toolchain not found: " + id.value());
    }

    if (it->isSelected()) {
        throw std::runtime_error("Cannot remove currently selected toolchain: " + id.value());
    }

    toolchains_.erase(it);
}

} // namespace scrap::toolchain::service