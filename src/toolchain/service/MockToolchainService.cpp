#include "ToolchainService.h"
#include <algorithm>
#include <sstream>
#include <system_error>

enum class ToolchainError {
    AlreadyInstalled = 1,
    NotFound,
    NotInstalled,
    CurrentlySelected
};

namespace std {
template<>
struct is_error_code_enum<ToolchainError> : true_type {};
}

class ToolchainErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override {
        return "toolchain";
    }

    std::string message(int ev) const override {
        switch (static_cast<ToolchainError>(ev)) {
            case ToolchainError::AlreadyInstalled: return "Toolchain is already installed";
            case ToolchainError::NotFound: return "Toolchain not found";
            case ToolchainError::NotInstalled: return "Toolchain is not installed";
            case ToolchainError::CurrentlySelected: return "Cannot remove currently selected toolchain";
            default: return "Unknown toolchain error";
        }
    }
};

const ToolchainErrorCategory& toolchainErrorCategory()
{
    static ToolchainErrorCategory instance;
    return instance;
}

std::error_code make_error_code(ToolchainError e)
{
    return {static_cast<int>(e), toolchainErrorCategory()};
}

namespace scrap::toolchain::service {

using namespace model;

// Base interface implementation
ToolchainService::~ToolchainService() = default;

MockToolchainService::MockToolchainService()
{
    initializeMockData();
}

MockToolchainService::~MockToolchainService() = default;

void MockToolchainService::initializeMockData()
{
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

std::vector<Toolchain> MockToolchainService::listInstalled()
{
    return toolchains_;
}

std::optional<Toolchain> MockToolchainService::getCurrentToolchain()
{
    if (!currentToolchainId_) {
        return std::nullopt;
    }
    return findById(*currentToolchainId_);
}

std::optional<Toolchain> MockToolchainService::findById(const ToolchainId& id)
{
    auto it = std::find_if(toolchains_.begin(), toolchains_.end(),
        [&id](const Toolchain& t) { return t.id() == id; });

    if (it != toolchains_.end()) {
        return *it;
    }
    return std::nullopt;
}

std::expected<void, std::string> MockToolchainService::install(const ToolchainSpecification& spec)
{
    // Check if already installed
    const std::string id = spec.name + "-" + spec.version + "-" +
                     architectureToString(spec.architecture.value_or(getCurrentArchitecture())) + "-" +
                     platformToString(spec.platform.value_or(getCurrentPlatform()));

    if (findById(ToolchainId(id))) {
        return std::unexpected("Toolchain " + id + " is already installed");
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
               << architectureToString(toolchain.architecture()) << "-"
               << platformToString(toolchain.platform());
    toolchain.setInstallationPath(pathStream.str());

    toolchains_.push_back(toolchain);
    return {};
}

std::expected<void, std::string> MockToolchainService::select(const ToolchainId& id)
{
    auto toolchain = findById(id);
    if (!toolchain) {
        return std::unexpected("Toolchain not found: " + id.value());
    }

    if (!toolchain->isInstalled()) {
        return std::unexpected("Toolchain is not installed: " + id.value());
    }

    // Update selection status
    for (auto& t : toolchains_) {
        t.setSelected(t.id() == id);
    }
    currentToolchainId_ = id;
    return {};
}

std::expected<void, std::string> MockToolchainService::remove(const ToolchainId& id)
{
    auto it = std::find_if(toolchains_.begin(), toolchains_.end(),
        [&id](const Toolchain& t) { return t.id() == id; });

    if (it == toolchains_.end()) {
        return std::unexpected("Toolchain not found: " + id.value());
    }

    if (it->isSelected()) {
        return std::unexpected("Cannot remove currently selected toolchain: " + id.value());
    }

    toolchains_.erase(it);
    return {};
}

} // namespace scrap::toolchain::service
