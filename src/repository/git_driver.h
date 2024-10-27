#pragma once

#include "repository.h"

#include <memory>

namespace scrap {

class git_driver : public repository::driver {
public:
    git_driver();
    ~git_driver();

    void clone(const std::string&, const std::filesystem::path&) override;
    void update(const std::filesystem::path&) override;

private:
    class internal;
    std::unique_ptr<internal> _impl;
};

}
