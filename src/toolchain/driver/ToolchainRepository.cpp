#include "toolchain/driver/ToolchainRepository.h"
#include "repository/model/Repository.h"
#include <dross/platform/path.h>
#include <dross/platform/xdg.h>
#include <filesystem>
#include <system_error>

namespace scrap::toolchain {

/**
 * @brief Private implementation class for GitToolchainRepository
 */
class GitToolchainRepository::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    std::vector<Toolchain> findAllInternal()
    {
        std::vector<Toolchain> toolchains;

        // TODO: Parse the actual toolchain registry and create Toolchain objects
        // For now, return some mock data to demonstrate the structure
        toolchains.emplace_back("gcc", "13.2.0", "x86_64");
        toolchains.emplace_back("llvm", "18.0.0", "x86_64");

        return toolchains;
    }

    std::unique_ptr<Toolchain> findDefaultInternal()
    {
        // TODO: Read default toolchain configuration
        // For now, return nullptr to indicate no default set
        return nullptr;
    }

    bool ensureRegistryAvailableInternal()
    {
        const auto directory = dross::xdg("scrap").data_home();
        if (!directory.has_value()) {
            return false;
        }

        if (!dross::path(directory.value()).exists()) {
            std::error_code err;
            if (!std::filesystem::create_directories(directory.value(), err)) {
                return false;
            }
        }

        const auto path = dross::path(directory.value()).append("toolchain");
        if (!path.exists()) {
            Repository(path).clone("https://github.com/skipbit/scrap-toolchain.git");
            return true;
        } else {
            Repository(path).update();
            return true;
        }
    }

    std::string getRegistryPathInternal()
    {
        const auto directory = dross::xdg("scrap").data_home();
        if (!directory.has_value()) {
            return "";
        }

        return dross::path(directory.value()).append("toolchain").string();
    }
};

// GitToolchainRepository implementation
GitToolchainRepository::GitToolchainRepository()
    : impl_(std::make_unique<Impl>())
{
}

GitToolchainRepository::~GitToolchainRepository() = default;

GitToolchainRepository::GitToolchainRepository(GitToolchainRepository&&) noexcept = default;

GitToolchainRepository& GitToolchainRepository::operator=(GitToolchainRepository&&) noexcept = default;

std::vector<Toolchain> GitToolchainRepository::findAll()
{
    return impl_->findAllInternal();
}

std::unique_ptr<Toolchain> GitToolchainRepository::findDefault()
{
    return impl_->findDefaultInternal();
}

bool GitToolchainRepository::ensureRegistryAvailable()
{
    return impl_->ensureRegistryAvailableInternal();
}

std::string GitToolchainRepository::getRegistryPath()
{
    return impl_->getRegistryPathInternal();
}

// ToolchainRepositoryFactory implementation
std::shared_ptr<ToolchainRepository> ToolchainRepositoryFactory::createRepository()
{
    return std::make_shared<GitToolchainRepository>();
}
