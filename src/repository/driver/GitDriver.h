#pragma once

#include "repository/driver/Driver.h"

#include <memory>

namespace scrap::repository {

class GitDriver : public Driver {
public:
    GitDriver();
    ~GitDriver();

    std::expected<void, std::string> clone(const std::string& url, const std::filesystem::path& path) override;
    std::expected<void, std::string> update(const std::filesystem::path& path) override;

private:
    class Internal;
    std::unique_ptr<Internal> impl_;
};

}
