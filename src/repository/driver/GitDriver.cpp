#include "repository/driver/GitDriver.h"
#include "repository/driver/LibGitRepository.h"

#include <git2.h>

namespace scrap::repository {

class GitDriver::Internal {
public:
    Internal() {
        git_libgit2_init();
    }
    ~Internal() {
        git_libgit2_shutdown();
    }

    std::unique_ptr<libgit::Repository> repository;
};

GitDriver::GitDriver()
    : impl_(std::make_unique<Internal>())
{
}

GitDriver::~GitDriver()
{
}

std::expected<void, std::string> GitDriver::clone(const std::string& url, const std::filesystem::path& path)
{
    try {
        auto repo = std::make_unique<libgit::Repository>(url, path);
        // Check if the repository was created successfully
        // This is a simplified check - in practice we'd need better error handling from libgit
        impl_->repository = std::move(repo);
        return {};
    } catch (const std::exception& e) {
        return std::unexpected("Failed to clone repository: " + std::string(e.what()));
    }
}

std::expected<void, std::string> GitDriver::update(const std::filesystem::path& path)
{
    try {
        if (!impl_->repository) {
            impl_->repository = std::make_unique<libgit::Repository>(path);
        }

        auto result = impl_->repository->update("origin", "main");
        if (!result) {
            return std::unexpected("Failed to update repository: " + result.error().message());
        }

        return {};
    } catch (const std::exception& e) {
        return std::unexpected("Failed to update repository: " + std::string(e.what()));
    }
}

}
