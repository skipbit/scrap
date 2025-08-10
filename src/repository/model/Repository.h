#pragma once

#include <string>
#include <filesystem>
#include <memory>

namespace scrap {

namespace repository { class Driver; }

class Repository {
public:
    enum class Type {
        Git,
    };

    Repository(const std::filesystem::path& directory);
    Repository(const Repository&);
    ~Repository();

    void clone(const std::string& url);
    void update();

private:
    std::shared_ptr<repository::Driver> driver_;
    std::filesystem::path directory_;
};

}
