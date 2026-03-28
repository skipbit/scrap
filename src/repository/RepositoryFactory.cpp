#include "repository/RepositoryFactory.h"
#include "repository/driver/GitDriver.h"

namespace scrap::repository {

std::unique_ptr<Repository> RepositoryFactory::createGitRepository(const std::filesystem::path& path)
{
    auto driver = std::make_shared<GitDriver>();
    return std::make_unique<Repository>(driver, path);
}

}  // namespace scrap::repository
