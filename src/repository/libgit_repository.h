#pragma once

#include <filesystem>
#include <memory>

namespace scrap {
namespace libgit {

class repository {
public:
    repository(const std::filesystem::path&);
    repository(const std::string&, const std::filesystem::path&);
    ~repository();

    void update(const std::string&, const std::string&);

private:
    class internal;
    std::unique_ptr<internal> _impl;
};

}
}
