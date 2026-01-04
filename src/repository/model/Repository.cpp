#include "repository/model/Repository.h"
#include "repository/driver/Driver.h"

namespace scrap {

Repository::Repository(std::shared_ptr<repository::Driver> driver,
                       const std::filesystem::path& path)
    : driver_(std::move(driver)), directory_(path)
{
}

Repository::Repository(const Repository& r)
    : driver_(r.driver_), directory_(r.directory_)
{
}

Repository::~Repository() = default;

void Repository::clone(const std::string& url)
{
    driver_->clone(url, directory_);
}

void Repository::update()
{
    driver_->update(directory_);
}

}
