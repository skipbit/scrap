#include "git_driver.h"
#include "libgit_repository.h"

#include <git2.h>

namespace scrap {

class git_driver::internal {
public:
    internal() {
        git_libgit2_init();
    }
    ~internal() {
        git_libgit2_shutdown();
    }

    std::unique_ptr<libgit::repository> repository;
};

git_driver::git_driver()
    : _impl(std::make_unique<internal>())
{
}

git_driver::~git_driver()
{
}

void git_driver::clone(const std::string& url, const std::filesystem::path& path)
{
    _impl->repository = std::make_unique<libgit::repository>(url, path);
}

void git_driver::update(const std::filesystem::path& path)
{
    if (! _impl->repository) {
        _impl->repository = std::make_unique<libgit::repository>(path);
    }
    _impl->repository->update("origin", "main");
}

}
