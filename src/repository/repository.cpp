#include "repository.h"
#include "libgit_repository.h"

namespace scrap {

repository repository::clone(const repository::type type, const std::string& url, const std::filesystem::path& directory)
{
    if (type == repository::type::git) {
        auto r = std::make_unique<libgit::repository>(url, directory);
    }
    return repository(type, directory);
}

repository::repository(const std::filesystem::path&)
{
}

repository::repository(const repository::type, const std::filesystem::path&)
{
}

repository::repository(const repository&)
{
}

void repository::update()
{
}

}
