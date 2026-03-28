#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace scrap {

namespace repository {
class Driver;
}

class Repository {
public:
    enum class Type {
        Git,
    };

    Repository(std::shared_ptr<repository::Driver> driver, const std::filesystem::path& directory);
    Repository(const Repository&);
    ~Repository();

    void clone(const std::string& url);
    void update();

private:
    std::shared_ptr<repository::Driver> driver_;
    std::filesystem::path directory_;
};

}  // namespace scrap
