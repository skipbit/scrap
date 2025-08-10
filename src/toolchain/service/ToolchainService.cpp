#include "toolchain/service/ToolchainService.h"
#include "toolchain/driver/ToolchainRepository.h"

namespace scrap::toolchain {

ToolchainService::ToolchainService(std::shared_ptr<ToolchainRepository> repository)
    : repository_(repository)
{
}

std::vector<Toolchain> ToolchainService::getAllToolchains()
{
    if (!repository_) {
        return {};
    }
    
    // Ensure registry is up-to-date before retrieving toolchains
    ensureRegistryUpToDate();
    
    return repository_->findAll();
}

std::unique_ptr<Toolchain> ToolchainService::getDefaultToolchain()
{
    if (!repository_) {
        return nullptr;
    }
    
    return repository_->findDefault();
}

bool ToolchainService::ensureRegistryUpToDate()
{
    if (!repository_) {
        return false;
    }
    
    return repository_->ensureRegistryAvailable();
}

}