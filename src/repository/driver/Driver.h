#pragma once

#include <string>
#include <filesystem>

namespace scrap::repository {

class Driver {
public:
    virtual ~Driver() = default;
    virtual void clone(const std::string& url, const std::filesystem::path& path) = 0;
    virtual void update(const std::filesystem::path& path) = 0;
};

}