#pragma once

#include "repository/driver/Driver.h"

#include <memory>

namespace scrap::repository {

class GitDriver : public Driver {
public:
    GitDriver();
    ~GitDriver();

    void clone(const std::string& url, const std::filesystem::path& path) override;
    void update(const std::filesystem::path& path) override;

private:
    class Internal;
    std::unique_ptr<Internal> impl_;
};

}
