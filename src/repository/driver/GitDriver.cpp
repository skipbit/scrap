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

void GitDriver::clone(const std::string& url, const std::filesystem::path& path)
{
    impl_->repository = std::make_unique<libgit::Repository>(url, path);
}

void GitDriver::update(const std::filesystem::path& path)
{
    if (! impl_->repository) {
        impl_->repository = std::make_unique<libgit::Repository>(path);
    }
    impl_->repository->update("origin", "main");
}

}
