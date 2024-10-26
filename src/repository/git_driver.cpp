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

}
