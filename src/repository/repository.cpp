#include "repository.h"
#include "git_driver.h"

namespace scrap {

repository::repository(const std::filesystem::path& path)
    : _driver(std::make_shared<git_driver>()), _directory(path)
{
}

repository::repository(const repository& r)
    : _driver(r._driver), _directory(r._directory)
{
}

repository::~repository() = default;

void repository::clone(const std::string& url)
{
    _driver->clone(url, _directory);
}

void repository::update()
{
    _driver->update(_directory);
}

}
