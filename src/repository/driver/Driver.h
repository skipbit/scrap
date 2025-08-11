#pragma once

#include <string>
#include <filesystem>
#include <expected>

namespace scrap::repository {

class Driver {
public:
    virtual ~Driver() = default;
    virtual std::expected<void, std::string> clone(const std::string& url, const std::filesystem::path& path) = 0;
    virtual std::expected<void, std::string> update(const std::filesystem::path& path) = 0;
};

}
